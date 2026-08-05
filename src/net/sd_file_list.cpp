#include "sd_file_list.h"
#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>

namespace
{
    bool hasGcodeExtension(const char *name)
    {
        size_t len = strlen(name);
        auto endsWithCi = [&](const char *suffix) {
            size_t slen = strlen(suffix);
            return len >= slen && strcasecmp(name + len - slen, suffix) == 0;
        };
        return endsWithCi(".gcode") || endsWithCi(".nc") || endsWithCi(".g");
    }
}

void SdFileList::beginCapture()
{
    buffer_ = "";
    ready_ = false;
    capturing_ = true;
    requestedAt_ = millis();
    Serial.println("[fluidnc] requesting SD file list ($SD/ListJSON=/)");
}

void SdFileList::feedLine(const char *line)
{
    if (!strcmp(line, "ok") || !strncmp(line, "error:", 6))
    {
        capturing_ = false;
        parseJson();
        return;
    }

    buffer_ += line;
    if (buffer_.length() > MAX_BUFFER_BYTES)
    {
        // Safety net: whatever is preventing the "ok"/"error:" terminator
        // from being recognized, never let this grow unbounded and
        // exhaust heap.
        Serial.println("[fluidnc] file list response exceeded size cap, aborting capture");
        capturing_ = false;
        buffer_ = "";
        ready_ = true;
    }
}

void SdFileList::checkTimeout()
{
    if (!capturing_) return;
    if (millis() - requestedAt_ <= TIMEOUT_MS) return;

    Serial.println("[fluidnc] file list request timed out, aborting capture");
    capturing_ = false;
    buffer_ = "";
    ready_ = true;
}

void SdFileList::parseJson()
{
    count_ = 0;

    Serial.printf("[fluidnc] file list raw response (%u bytes): %s\n",
                  (unsigned)buffer_.length(), buffer_.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, buffer_);
    if (err)
    {
        Serial.printf("[fluidnc] file list JSON parse failed: %s\n", err.c_str());
        ready_ = true;
        return;
    }

    for (JsonObject f : doc["files"].as<JsonArray>())
    {
        if (count_ >= MAX_FILES) break;

        const char *name = f["name"] | "";

        // Observed on live hardware: this FluidNC build emits "size" as a
        // quoted string (e.g. "size":"100181"), not a JSON number as the
        // upstream source (FileCommands.cpp) would suggest -- accept either
        // shape rather than trust one representation.
        int32_t size = -1;
        JsonVariantConst sizeVal = f["size"];
        if (sizeVal.is<const char *>()) size = atoi(sizeVal.as<const char *>());
        else size = sizeVal | -1;

        if (size < 0) continue;              // directories: skip, flat list only
        if (!hasGcodeExtension(name)) continue;

        FluidNCFileEntry &entry = files_[count_++];
        strncpy(entry.name, name, sizeof(entry.name) - 1);
        entry.name[sizeof(entry.name) - 1] = '\0';
        entry.size = size;
    }

    Serial.printf("[fluidnc] file list parsed: %d g-code file(s)\n", count_);
    ready_ = true;
}
