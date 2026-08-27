#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "machine_mode.h"
#include "sd_file_list.h"

// Live state parsed out of FluidNC's realtime status reports
// (`<State|MPos:...|WCO:...|FS:...|SD:pct,filename>`). Field meanings and
// the WCO-latching approach are carried over from terraPixel's proven
// parser (warderoid-ctrl/terraPixel, src/main.cpp) -- same math, different
// transport (WebSocket here vs. telnet there).
struct FluidNCStatus
{
    bool connected = false;
    MachineMode mode = MachineMode::Boot;

    bool havePos = false;
    float wposX = 0, wposY = 0, wposZ = 0;

    // -1 when FluidNC isn't reporting an SD job percentage (e.g. not
    // running from SD, or this firmware build doesn't emit the field --
    // see the plan's note on verifying the `SD:` field against your
    // specific FluidNC version).
    float jobPercent = -1;
    char jobFilename[48] = {0};

    // True only for a real SD-file job started via runFile() -- distinct
    // from MachineMode::Run, which FluidNC also reports for plain jogging
    // ("Jog" state maps to Run too, see applyState()). ui_nav uses this to
    // gate its auto-navigation to/from Job Progress so a knob jog doesn't
    // get mistaken for a job starting.
    bool jobActive = false;

    // The last thing FluidNC said that wasn't a status report or a bare
    // "ok" -- an "error:N", an "ALARM:N", or an "[MSG:...]". Status reports
    // only carry the *state* (Alarm), never the reason, so without this an
    // intermittent fault is invisible to anyone without a serial cable
    // attached. Shown on the alarm screen.
    char lastMessage[56] = {0};
    uint32_t lastMessageAt = 0;

    // True when lastMessage is an actual failure ("error:N"/"ALARM:N" or a
    // dropped socket) rather than ordinary chatter. Failures are held for a
    // while so later chatter can't bury them -- see noteMessage().
    bool lastFailure = false;

    // Bumped on every websocket drop. A pendant that loses its channel
    // mid-cycle and silently reconnects looks, from the outside, exactly
    // like a machine that alarmed on its own -- this tells the two apart.
    uint16_t disconnects = 0;
};

// THREADING: update() does genuinely blocking work -- mDNS resolution
// (seconds), and WebSocket frame reads that spin until the rest of a
// partially-arrived frame shows up. It therefore runs on main.cpp's
// dedicated networkTask (core 0), NEVER on the UI/LVGL loop, which is what
// used to make the whole panel stutter whenever the plotter was connected.
//
// Everything else here is safe to call from the UI task: the command
// methods only enqueue text for networkTask to transmit (they never touch
// the WebSocket, which is not thread-safe), status() is a plain read of a
// struct networkTask updates, and the file-list accessors take a short
// mutex.
class FluidNCClient
{
public:
    // Call once from setup(), before networkTask starts -- creates the
    // command queue and file-list mutex so UI-thread callers always have
    // somewhere to put commands, even before WiFi is up.
    void initTransport();

    // Call once, after WiFi is connected.
    void begin();

    // networkTask only. Handles mDNS resolution, (re)connection, draining
    // the outbound command queue, and pumping received frames.
    void update();

    const FluidNCStatus &status() const { return status_; }

    // Realtime single-byte commands (sent immediately, no line buffering).
    void requestStatus(); // '?'
    void feedHold();      // '!'
    void resume();        // '~'
    void softReset();     // 0x18

    // Line commands.
    // home() is state-aware: it only unlocks first if the machine is
    // actually alarmed, and then holds the $H back until the unlock has
    // landed. See the comment on the definition for why.
    void home();                                       // $X (if alarmed) then $H
    void jog(char axis, float deltaMm, float feedrate); // $J=G91 G21 <axis><delta> F<feed>
    void clearAlarm();                                  // $X
    void runFile(const char *filename);                 // $SD/Run=<file>
    void deleteFile(const char *filename);              // $SD/Delete=<file>
    void sendGcodeLine(const char *line);                // arbitrary line (pen macros, etc.)

    // SD file listing. requestFileList() sends `$SD/ListJSON=/` and starts
    // capturing the (multi-line) JSON response; poll fileListReady() and
    // call clearFileListReady() once you've read the results via
    // fileListCount()/fileListEntry(). Only files with a recognized G-code
    // extension are kept; directories are skipped (flat pendant list, no
    // subfolder browsing).
    static const int MAX_FILES = SdFileList::MAX_FILES;
    void requestFileList();
    bool fileListReady() const;
    void clearFileListReady();
    int fileListCount() const;
    // Copies entry i into out; returns false if i is out of range. Copies
    // rather than returning a reference because networkTask may rewrite the
    // underlying array the moment the mutex is released.
    bool fileListEntry(int i, FluidNCFileEntry &out) const;

    // Internal: bridges the C-style WebSocketsClient event callback back
    // into this instance. Public only because the trampoline needs it;
    // not part of the intended API surface.
    void onWsEvent(uint8_t type, uint8_t *payload, size_t length);

private:
    FluidNCStatus status_;

    bool wsBegun_ = false;
    uint32_t lastResolveAttempt_ = 0;
    IPAddress resolvedIp_;

    char lineBuf_[192];
    size_t lineLen_ = 0;

    // Deferred half of home(): $X goes out immediately, $H waits for
    // FluidNC to actually leave Alarm (or for the timeout below).
    bool homePending_ = false;
    uint32_t homeRequestedAt_ = 0;
    static const uint32_t UNLOCK_SETTLE_MS = 150;
    static const uint32_t UNLOCK_TIMEOUT_MS = 2000;

    uint32_t runStartedAt_ = 0;
    uint32_t doneAt_ = 0;
    static const uint32_t MIN_JOB_MS = 5000;
    static const uint32_t CELEBRATE_MS = 12000;

    SdFileList sdFiles_;

    // Outbound command plumbing. UI-thread callers enqueue; networkTask
    // dequeues and transmits. `raw` distinguishes realtime single bytes
    // ('?', '!', 0x18, 0x85) from newline-terminated line commands.
    struct OutCmd
    {
        bool raw;
        char text[112];
    };
    static const int CMD_QUEUE_DEPTH = 12;
    QueueHandle_t cmdQueue_ = nullptr;
    mutable SemaphoreHandle_t fileMutex_ = nullptr;

    // Filters what FluidNC says down to what's worth showing on a 240px
    // screen, and keeps failures from being buried by chatter.
    void noteMessage(const char *line);
    static const uint32_t FAILURE_STICKY_MS = 20000;

    void enqueue(bool raw, const char *text);
    void drainCommandQueue();
    void servicePendingHome();

    bool resolveHost();
    void sendRaw(const char *s);
    void sendLine(const String &line);
    void ingest(const char *data, size_t len);
    void handleLine(char *line);
    void applyState(const char *state);
};

extern FluidNCClient fluidNC;
