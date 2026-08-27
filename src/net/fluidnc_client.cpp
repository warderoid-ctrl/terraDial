#include "fluidnc_client.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebSocketsClient.h>
#include <string.h>
#include "../config/settings.h"

FluidNCClient fluidNC;

static WebSocketsClient wsClient;

// WebSocketsClient's onEvent wants a plain function pointer -- it has no
// notion of a bound member function, so this trampoline forwards to the
// (single) FluidNCClient instance.
static void wsEventTrampoline(WStype_t type, uint8_t *payload, size_t length)
{
    fluidNC.onWsEvent((uint8_t)type, payload, length);
}

void FluidNCClient::initTransport()
{
    if (!cmdQueue_) cmdQueue_ = xQueueCreate(CMD_QUEUE_DEPTH, sizeof(OutCmd));
    if (!fileMutex_) fileMutex_ = xSemaphoreCreateMutex();
}

void FluidNCClient::begin()
{
    // mDNS is started once in main.cpp (shared with TerraPixelClient)
    // before this is used -- nothing to do here.
}

void FluidNCClient::enqueue(bool raw, const char *text)
{
    if (!cmdQueue_) return; // initTransport() not called yet -- nothing to do but drop
    OutCmd cmd;
    cmd.raw = raw;
    strncpy(cmd.text, text, sizeof(cmd.text) - 1);
    cmd.text[sizeof(cmd.text) - 1] = '\0';
    // Never block the UI task waiting for queue space: if the network task
    // is wedged behind a slow socket, dropping a jog/status command is far
    // better than freezing the display until it recovers.
    if (xQueueSend(cmdQueue_, &cmd, 0) != pdTRUE)
        Serial.printf("[fluidnc] command queue full, dropped: %s\n", text);
}

void FluidNCClient::drainCommandQueue()
{
    if (!cmdQueue_) return;
    OutCmd cmd;
    while (xQueueReceive(cmdQueue_, &cmd, 0) == pdTRUE)
    {
        if (cmd.raw) sendRaw(cmd.text);
        else sendLine(String(cmd.text));
    }
}

bool FluidNCClient::fileListReady() const
{
    if (!fileMutex_) return false;
    bool v = false;
    if (xSemaphoreTake(fileMutex_, pdMS_TO_TICKS(5)) == pdTRUE)
    {
        v = sdFiles_.ready();
        xSemaphoreGive(fileMutex_);
    }
    return v;
}

void FluidNCClient::clearFileListReady()
{
    if (!fileMutex_) return;
    if (xSemaphoreTake(fileMutex_, pdMS_TO_TICKS(5)) == pdTRUE)
    {
        sdFiles_.clearReady();
        xSemaphoreGive(fileMutex_);
    }
}

int FluidNCClient::fileListCount() const
{
    if (!fileMutex_) return 0;
    int n = 0;
    if (xSemaphoreTake(fileMutex_, pdMS_TO_TICKS(5)) == pdTRUE)
    {
        n = sdFiles_.count();
        xSemaphoreGive(fileMutex_);
    }
    return n;
}

bool FluidNCClient::fileListEntry(int i, FluidNCFileEntry &out) const
{
    if (!fileMutex_) return false;
    bool ok = false;
    if (xSemaphoreTake(fileMutex_, pdMS_TO_TICKS(5)) == pdTRUE)
    {
        if (i >= 0 && i < sdFiles_.count())
        {
            out = sdFiles_.entry(i);
            ok = true;
        }
        xSemaphoreGive(fileMutex_);
    }
    return ok;
}

bool FluidNCClient::resolveHost()
{
    IPAddress ip = MDNS.queryHost(Config::get().fluidNcHost, 2000);
    if (ip == IPAddress((uint32_t)0)) return false;
    resolvedIp_ = ip;
    return true;
}

void FluidNCClient::update()
{
    if (WiFi.status() != WL_CONNECTED) return;

    if (!wsBegun_)
    {
        uint32_t now = millis();
        if (now - lastResolveAttempt_ < 3000) return;
        lastResolveAttempt_ = now;

        if (!resolveHost())
        {
            Serial.printf("[fluidnc] mDNS lookup for %s.local failed, retrying\n", Config::get().fluidNcHost);
            return;
        }

        Serial.printf("[fluidnc] resolved %s.local -> %s, opening websocket\n", Config::get().fluidNcHost, resolvedIp_.toString().c_str());
        // FluidNC's AsyncWebSocket is mounted at "/" on the same port as its
        // HTTP server (default 80) -- there is no separate port 81 in the
        // current FluidNC source (WebUI/WebUIServer.cpp), unlike the older
        // ESP3D-webui convention this was originally modeled on.
        wsClient.begin(resolvedIp_.toString().c_str(), 80, "/");
        wsClient.onEvent(wsEventTrampoline);
        wsClient.setReconnectInterval(3000);
        wsBegun_ = true;
        return;
    }

    servicePendingHome(); // may enqueue, so before the drain
    drainCommandQueue();
    wsClient.loop();
    if (fileMutex_ && xSemaphoreTake(fileMutex_, pdMS_TO_TICKS(5)) == pdTRUE)
    {
        sdFiles_.checkTimeout();
        xSemaphoreGive(fileMutex_);
    }
}

void FluidNCClient::onWsEvent(uint8_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
        case WStype_CONNECTED:
            Serial.println("[fluidnc] websocket connected");
            status_.connected = true;
            // Re-issued on every (re)connect -- auto-reporting is per-channel
            // and FluidNC forgets it across disconnects.
            sendLine("$Report/Interval=100");
            sendRaw("?");
            break;

        case WStype_DISCONNECTED:
            Serial.println("[fluidnc] websocket disconnected");
            status_.connected = false;
            status_.disconnects++;
            strncpy(status_.lastMessage, "websocket dropped", sizeof(status_.lastMessage) - 1);
            status_.lastMessageAt = millis();
            status_.lastFailure = true;
            status_.mode = MachineMode::Boot;
            status_.havePos = false;
            if (fileMutex_ && xSemaphoreTake(fileMutex_, pdMS_TO_TICKS(5)) == pdTRUE)
            {
                sdFiles_.abort();
                xSemaphoreGive(fileMutex_);
            }
            break;

        case WStype_TEXT:
        case WStype_BIN:
        // A message too large for one WebSocket frame arrives as separate
        // fragment events instead of TEXT/BIN -- the SD file listing is the
        // one response big enough to hit this (everything else: status
        // reports, command acks, is small enough to always land as a single
        // TEXT/BIN frame). ingest() just accumulates raw bytes into a line
        // buffer regardless of frame boundaries, so fragments feed through
        // it exactly like TEXT/BIN; without these cases every fragmented
        // message -- in practice, only the file listing -- was silently
        // dropped, which is why Jobs always timed out with literally no
        // data captured while everything else worked fine.
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            ingest((const char *)payload, length);
            break;

        default:
            break;
    }
}

void FluidNCClient::sendRaw(const char *s)
{
    if (!status_.connected) return;
    wsClient.sendTXT(s);
}

void FluidNCClient::sendLine(const String &line)
{
    if (!status_.connected)
    {
        Serial.printf("[fluidnc] sendLine(\"%s\") dropped -- not connected\n", line.c_str());
        return;
    }
    wsClient.sendTXT(line + "\n");
}

void FluidNCClient::ingest(const char *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        char c = data[i];
        if (c == '\n' || c == '\r')
        {
            if (lineLen_)
            {
                lineBuf_[lineLen_] = '\0';
                handleLine(lineBuf_);
                lineLen_ = 0;
            }
        }
        else if (lineLen_ < sizeof(lineBuf_) - 1)
        {
            lineBuf_[lineLen_++] = c;
        }
    }
}

void FluidNCClient::applyState(const char *state)
{
    MachineMode next = status_.mode;

    if      (!strncmp(state, "Run",   3)) next = MachineMode::Run;
    else if (!strncmp(state, "Jog",   3)) next = MachineMode::Run;
    else if (!strncmp(state, "Hold",  4)) next = MachineMode::Hold;
    else if (!strncmp(state, "Door",  4)) next = MachineMode::Hold;
    else if (!strncmp(state, "Alarm", 5)) next = MachineMode::Alarm;
    else if (!strncmp(state, "Home",  4)) next = MachineMode::Homing;
    else if (!strncmp(state, "Sleep", 5)) next = MachineMode::Idle;
    else if (!strncmp(state, "Idle",  4))
    {
        bool wasJob = (status_.mode == MachineMode::Run) && (millis() - runStartedAt_ > MIN_JOB_MS);
        if (wasJob) { next = MachineMode::Done; doneAt_ = millis(); }
        else if (status_.mode == MachineMode::Done && millis() - doneAt_ < CELEBRATE_MS) next = MachineMode::Done;
        else next = MachineMode::Idle;
    }

    if (next == MachineMode::Run && status_.mode != MachineMode::Run) runStartedAt_ = millis();
    if (next != MachineMode::Run && next != MachineMode::Hold) status_.jobActive = false;
    status_.mode = next;
}

// Everything FluidNC says goes to the serial log, but only some of it is
// worth putting on the alarm screen.
void FluidNCClient::noteMessage(const char *line)
{
    bool failure = !strncmp(line, "error:", 6) || !strncmp(line, "ALARM:", 6);

    if (!failure)
    {
        // A verbose FluidNC build narrates homing at about 25
        // [MSG:DBG:...] lines per cycle. Invaluable on the serial log,
        // fatal on the alarm screen: the chatter continues after an
        // aborted cycle, so an unfiltered "last message" would calmly
        // report "Homing done" over the ALARM that actually stopped you.
        if (!strncmp(line, "[MSG:DBG:", 9)) return;

        // Same reasoning, slower: don't let ordinary messages overwrite a
        // fresh failure before anyone has walked over to read it.
        if (status_.lastFailure && millis() - status_.lastMessageAt < FAILURE_STICKY_MS) return;
    }

    strncpy(status_.lastMessage, line, sizeof(status_.lastMessage) - 1);
    status_.lastMessage[sizeof(status_.lastMessage) - 1] = '\0';
    status_.lastMessageAt = millis();
    status_.lastFailure = failure;
}

void FluidNCClient::handleLine(char *line)
{
    // Auto-reporting (`$Report/Interval=100`) keeps sending realtime status
    // lines on this same channel throughout a file-list capture -- those
    // must still flow through to the normal status parsing below, not get
    // appended to the JSON buffer (which would both corrupt the JSON and,
    // if it ever prevented us from recognizing "ok", grow the buffer
    // forever). Only non-status lines are treated as part of the capture.
    if (sdFiles_.isCapturing() && line[0] != '<')
    {
        // FluidNC's JSONencoder flushes at structural boundaries (each
        // array element, each object close), so the `$SD/ListJSON` response
        // arrives as many separate lines that only form valid JSON once
        // concatenated -- SdFileList accumulates until the trailing
        // "ok"/"error:" that FluidNC's Channel::ack() sends after every
        // command completes.
        if (fileMutex_ && xSemaphoreTake(fileMutex_, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            sdFiles_.feedLine(line);
            xSemaphoreGive(fileMutex_);
        }
        return;
    }

    // Everything that isn't a status report is an ack: "ok", "error:N",
    // "ALARM:N" or an "[MSG:...]". Nothing here parses them, but they are
    // the only place FluidNC says *why* something failed, so at least put
    // them on the serial log rather than dropping them silently.
    if (line[0] != '<')
    {
        if (strncmp(line, "ok", 2) != 0)
        {
            Serial.printf("[fluidnc] %s\n", line);
            noteMessage(line);
        }
        return;
    }

    char *end = strpbrk(line + 1, "|>");
    if (!end) return;
    char saved = *end;
    *end = '\0';
    applyState(line + 1);
    *end = saved;

    static float wcoX = 0, wcoY = 0, wcoZ = 0;
    char *wco = strstr(end, "WCO:");
    if (wco) sscanf(wco + 4, "%f,%f,%f", &wcoX, &wcoY, &wcoZ);

    char *wpos = strstr(end, "WPos:");
    if (wpos)
    {
        sscanf(wpos + 5, "%f,%f,%f", &status_.wposX, &status_.wposY, &status_.wposZ);
        status_.havePos = true;
    }
    else
    {
        char *mpos = strstr(end, "MPos:");
        if (mpos)
        {
            float mx, my, mz;
            sscanf(mpos + 5, "%f,%f,%f", &mx, &my, &mz);
            status_.wposX = mx - wcoX;
            status_.wposY = my - wcoY;
            status_.wposZ = mz - wcoZ;
            status_.havePos = true;
        }
    }

    // Tentative: FluidNC's SD-job-percentage field. Confirm the exact
    // "SD:<pct>,<filename>" syntax against your firmware build -- if it's
    // absent or shaped differently, this just leaves jobPercent at -1.
    char *sd = strstr(end, "SD:");
    if (sd)
    {
        status_.jobPercent = atof(sd + 3);

        // FluidNC only reports SD: while it is actually running a file, so
        // its presence is proof of a job in a way mode==Run isn't (Run also
        // covers plain jogging). runFile() sets this too, but only for jobs
        // *we* started -- a job launched from the web UI or terraForge left
        // the pendant showing an empty ring at 0%, which is the whole point
        // of having a pendant missed. Gated on Run/Hold so a trailing SD:
        // in a post-job status report can't keep the flag raised.
        if (status_.mode == MachineMode::Run || status_.mode == MachineMode::Hold)
            status_.jobActive = true;
        char *comma = strchr(sd + 3, ',');
        if (comma)
        {
            comma++;
            size_t i = 0;
            while (*comma && *comma != '|' && *comma != '>' && i < sizeof(status_.jobFilename) - 1)
                status_.jobFilename[i++] = *comma++;
            status_.jobFilename[i] = '\0';
        }
    }
    else
    {
        status_.jobPercent = -1;
        status_.jobFilename[0] = '\0';
    }
}

// All of these are called from the UI task -- they only enqueue, so a
// stalled socket can never stall a button press (see the THREADING note in
// the header).
void FluidNCClient::requestStatus() { enqueue(true, "?"); }
void FluidNCClient::feedHold()      { enqueue(true, "!"); }
void FluidNCClient::resume()        { enqueue(true, "~"); }
void FluidNCClient::softReset()     { enqueue(true, "\x18"); }

// Homing used to be an unconditional "$X then $H" pair enqueued together,
// which meant both lines hit the websocket in the same drain, microseconds
// apart, whatever state the machine was in. That is the one thing the
// pendant did differently from the web UI -- and it broke exactly the case
// where the $X is pointless: homing a machine that is already Idle because
// it just homed. Unlocking pokes FluidNC's state machine, and a $H that
// arrives in the same breath can start a homing cycle that the pending
// unlock then aborts, which surfaces as an instant alarm. Homing once from
// a fresh (alarmed) boot always worked because there the $X had real work
// to do and was resolved before the $H landed.
//
// So: unlock only when there is an alarm to clear, and never in the same
// burst as the $H.
void FluidNCClient::home()
{
    // A second $H mid-cycle can only interfere with the one already
    // running -- the machine is going to the same place regardless.
    if (status_.mode == MachineMode::Homing) return;

    if (status_.mode == MachineMode::Alarm)
    {
        enqueue(false, "$X");
        homePending_ = true;
        homeRequestedAt_ = millis();
        return;
    }

    enqueue(false, "$H");
}

// networkTask only -- releases the $H held back by home().
void FluidNCClient::servicePendingHome()
{
    if (!homePending_) return;

    uint32_t waited = millis() - homeRequestedAt_;
    if (waited < UNLOCK_SETTLE_MS) return;
    if (status_.mode == MachineMode::Alarm && waited < UNLOCK_TIMEOUT_MS) return;

    if (status_.mode == MachineMode::Alarm)
        Serial.println("[fluidnc] still alarmed after $X, homing anyway -- watch for the error below");

    homePending_ = false;
    enqueue(false, "$H");
}
void FluidNCClient::clearAlarm() { enqueue(false, "$X"); }

void FluidNCClient::jog(char axis, float deltaMm, float feedrate)
{
    // A homing cycle is not something to jog out from under: 0x85 below is
    // a motion-cancel realtime byte, so sent mid-cycle it stops the homing
    // move, and FluidNC alarms on a cycle that didn't finish. The knob is
    // live on the Jog and Pen screens the whole time homing runs, so this
    // is one nudge away rather than hypothetical.
    if (status_.mode == MachineMode::Homing) return;

    // Jog cancel (GRBL/FluidNC realtime byte 0x85) first: without it,
    // FluidNC keeps running the previous jog move to completion before a
    // new $J= line takes effect, so alternating commands (knob jogging
    // back and forth, tapping Pen up then Pen down) felt laggy -- each new
    // press had to wait out whatever motion was still in flight. Cancelling
    // first lets a new jog interrupt the old one immediately.
    enqueue(true, "\x85");
    String cmd = "$J=G91 G21 ";
    cmd += axis;
    cmd += String(deltaMm, 3);
    cmd += " F";
    cmd += String(feedrate, 0);
    enqueue(false, cmd.c_str());
}

void FluidNCClient::requestFileList()
{
    if (fileMutex_ && xSemaphoreTake(fileMutex_, pdMS_TO_TICKS(5)) == pdTRUE)
    {
        sdFiles_.beginCapture();
        xSemaphoreGive(fileMutex_);
    }
    // Unencapsulated JSON (no [MSG:JSON:...] wrapper, unlike Files/ListGCode)
    // -- non-recursive listing of the SD root.
    enqueue(false, "$SD/ListJSON=/");
}

void FluidNCClient::runFile(const char *filename)
{
    status_.jobActive = true;
    String cmd = "$SD/Run=";
    cmd += filename;
    enqueue(false, cmd.c_str());
}

void FluidNCClient::deleteFile(const char *filename)
{
    String cmd = "$SD/Delete=";
    cmd += filename;
    enqueue(false, cmd.c_str());
}

void FluidNCClient::sendGcodeLine(const char *line) { enqueue(false, line); }
