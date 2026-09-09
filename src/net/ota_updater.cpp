#include "ota_updater.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <string.h>

#include "branding.h"
#include "version.h"
#include "fluidnc_client.h"

namespace
{
    // Written by the network task, read by the UI -- see the threading note
    // in ota_updater.h.
    volatile OtaUpdater::State state_ = OtaUpdater::State::Idle;
    volatile bool checkRequested_ = false;
    volatile bool installRequested_ = false;
    volatile int progressPct_ = 0;
    volatile bool restartPending_ = false;

    char latestVersion_[24] = "";
    char downloadUrl_[256] = "";
    char message_[96] = "";

    void setMessage(const char *m)
    {
        strncpy(message_, m, sizeof(message_) - 1);
        message_[sizeof(message_) - 1] = '\0';
    }

    void fail(const char *m)
    {
        setMessage(m);
        state_ = OtaUpdater::State::Failed;
        Serial.printf("[ota] failed: %s\n", m);
    }

    // "1.2.3" -> 1002003, so a plain integer compare orders releases.
    // Anything past the third component (an "-rc1" suffix, say) is ignored:
    // this is a hobby firmware with linear releases, and implementing semver
    // precedence properly would be more code than the problem has.
    uint32_t versionRank(const char *v)
    {
        while (*v == 'v' || *v == 'V') v++;
        uint32_t part[3] = {0, 0, 0};
        int i = 0;
        while (*v && i < 3)
        {
            if (*v >= '0' && *v <= '9') part[i] = part[i] * 10 + (uint32_t)(*v - '0');
            else if (*v == '.') i++;
            else break;
            v++;
        }
        return part[0] * 1000000u + part[1] * 1000u + part[2];
    }

    // GitHub's certificate chain rotates, and a panel that bricks its own
    // update path the day a root expires is worse than one that doesn't pin.
    // The payload is verified where it actually matters: Update.h checks the
    // image's own header and its MD5 before it will boot it, so a corrupted
    // or truncated download fails to flash rather than half-installing.
    void configureClient(WiFiClientSecure &client)
    {
        client.setInsecure();
        client.setTimeout(15); // seconds
    }

    // Fills latestVersion_/downloadUrl_ from the newest published release.
    bool fetchLatestRelease()
    {
        WiFiClientSecure client;
        configureClient(client);

        char url[160];
        snprintf(url, sizeof(url), "https://api.github.com/repos/%s/%s/releases/latest",
                 Branding::githubOwner(), Branding::githubRepo());

        HTTPClient http;
        http.setTimeout(15000);
        http.setUserAgent("terraDial"); // GitHub's API rejects a request with no User-Agent
        // Renaming the repo doesn't break the URL, it turns it into a 301 --
        // and GitHub answers that with a JSON body explaining the move, not
        // with the release. Following redirects here is what stops a rename
        // silently killing the update path on every panel already in the
        // field, which is the one failure nobody can fix remotely.
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        if (!http.begin(client, url))
        {
            fail("Couldn't reach GitHub");
            return false;
        }
        http.addHeader("Accept", "application/vnd.github+json");

        int code = http.GET();
        if (code != HTTP_CODE_OK)
        {
            // 404 is the common one and it isn't a network fault: it means
            // the repo has no published release yet (or was renamed). Say
            // that, rather than "HTTP 404" on a 240px screen.
            if (code == HTTP_CODE_NOT_FOUND) fail("No releases published yet");
            else if (code == HTTP_CODE_FORBIDDEN) fail("GitHub rate limit -- try later");
            else
            {
                char m[48];
                snprintf(m, sizeof(m), "Check failed (HTTP %d)", code);
                fail(m);
            }
            http.end();
            return false;
        }

        // The release JSON runs to tens of KB of fields we don't want. The
        // filter keeps the parse to a few hundred bytes, which matters on a
        // board that has just spent ~40KB of heap standing up TLS.
        JsonDocument filter;
        filter["tag_name"] = true;
        filter["assets"][0]["name"] = true;
        filter["assets"][0]["browser_download_url"] = true;

        JsonDocument doc;
        DeserializationError err =
            deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
        http.end();

        if (err)
        {
            fail("Couldn't read release info");
            return false;
        }

        const char *tag = doc["tag_name"] | "";
        if (!tag[0])
        {
            fail("Release has no version tag");
            return false;
        }
        while (*tag == 'v' || *tag == 'V') tag++;
        strncpy(latestVersion_, tag, sizeof(latestVersion_) - 1);
        latestVersion_[sizeof(latestVersion_) - 1] = '\0';

        // The OTA image is published under a fixed name (see the release
        // workflow) precisely so this doesn't have to reconstruct a
        // version-stamped filename from the tag.
        downloadUrl_[0] = '\0';
        for (JsonObject asset : doc["assets"].as<JsonArray>())
        {
            const char *name = asset["name"] | "";
            if (strcmp(name, OTA_ASSET_NAME) != 0) continue;
            const char *dl = asset["browser_download_url"] | "";
            strncpy(downloadUrl_, dl, sizeof(downloadUrl_) - 1);
            downloadUrl_[sizeof(downloadUrl_) - 1] = '\0';
            break;
        }

        if (!downloadUrl_[0])
        {
            fail("Release has no firmware file");
            return false;
        }
        return true;
    }

    void doCheck()
    {
        state_ = OtaUpdater::State::Checking;
        setMessage("Checking...");

        if (!fetchLatestRelease()) return;

        Serial.printf("[ota] latest release %s (running %s)\n", latestVersion_, Version::firmware());

        // A dev build has no meaningful place in the version ordering, so it
        // is always offered the published release rather than compared
        // against it -- see version.h.
        bool newer = Version::isDevBuild() ||
                     versionRank(latestVersion_) > versionRank(Version::firmware());

        if (newer)
        {
            char m[64];
            snprintf(m, sizeof(m), "Version %s available", latestVersion_);
            setMessage(m);
            state_ = OtaUpdater::State::Available;
        }
        else
        {
            setMessage("You're up to date");
            state_ = OtaUpdater::State::UpToDate;
        }
    }

    void onProgress(size_t done, size_t total)
    {
        int pct = total ? (int)((done * 100ULL) / total) : 0;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        progressPct_ = pct;
    }

    void doInstall()
    {
        if (!downloadUrl_[0])
        {
            fail("Nothing to install");
            return;
        }

        // The download blocks this task for the length of the transfer, and
        // this task is the only thing pumping FluidNC's WebSocket -- so a
        // running job would lose its status stream, and its progress screen,
        // for a minute or more. The reboot at the end drops the connection
        // outright. The machine finishing its work wins.
        const FluidNCStatus &st = fluidNC.status();
        if (st.jobActive || st.mode == MachineMode::Run || st.mode == MachineMode::Homing ||
            st.mode == MachineMode::Hold)
        {
            fail("Machine busy -- try when idle");
            return;
        }

        state_ = OtaUpdater::State::Installing;
        progressPct_ = 0;
        setMessage("Downloading...");
        Serial.printf("[ota] installing from %s\n", downloadUrl_);

        WiFiClientSecure client;
        configureClient(client);

        Update.onProgress(onProgress);
        httpUpdate.rebootOnUpdate(false);                             // the UI gets to say "restarting" first; main.cpp does the reboot
        httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // release assets redirect to objects.githubusercontent.com

        t_httpUpdate_return ret = httpUpdate.update(client, downloadUrl_, Version::firmware());
        switch (ret)
        {
            case HTTP_UPDATE_OK:
                progressPct_ = 100;
                setMessage("Installed -- restarting");
                state_ = OtaUpdater::State::Done;
                restartPending_ = true;
                break;

            case HTTP_UPDATE_NO_UPDATES:
                // Only reachable if the server declined the request; the
                // version check above already ruled out "same version".
                setMessage("You're up to date");
                state_ = OtaUpdater::State::UpToDate;
                break;

            default:
            {
                char m[80];
                snprintf(m, sizeof(m), "Install failed: %s",
                         httpUpdate.getLastErrorString().c_str());
                fail(m);
                break;
            }
        }
    }
}

namespace OtaUpdater
{
    void requestCheck()
    {
        if (state_ == State::Checking || state_ == State::Installing) return;
        checkRequested_ = true;
    }

    void requestInstall()
    {
        if (state_ != State::Available) return;
        installRequested_ = true;
    }

    void dismiss()
    {
        if (state_ == State::Checking || state_ == State::Installing || state_ == State::Done) return;
        state_ = State::Idle;
        message_[0] = '\0';
    }

    void update()
    {
        if (installRequested_)
        {
            installRequested_ = false;
            checkRequested_ = false; // an install supersedes a queued check
            doInstall();
            return;
        }
        if (checkRequested_)
        {
            checkRequested_ = false;
            doCheck();
        }
    }

    State state() { return state_; }
    int progressPct() { return progressPct_; }
    const char *latestVersion() { return latestVersion_; }
    const char *message() { return message_; }
    bool restartPending() { return restartPending_; }
}
