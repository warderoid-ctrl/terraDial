#pragma once

#include <Arduino.h>

// FluidNC's SD-card gcode file list ($SD/ListJSON=/). Split out of
// FluidNCClient because it's a fairly separate concern from realtime
// status/connection handling: FluidNC's JSONencoder flushes at structural
// boundaries, so the response arrives as many separate lines that only
// form valid JSON once concatenated, and only forms complete once the
// trailing "ok"/"error:" that FluidNC's Channel::ack() sends after every
// command completes is seen. This class owns that accumulate-then-parse
// state machine in isolation from status-line parsing.
struct FluidNCFileEntry
{
    char name[48] = {0};
    int32_t size = -1;
};

class SdFileList
{
public:
    static const int MAX_FILES = 40;

    // Starts a new capture: resets buffer/ready state and records the
    // request time for the timeout check. Caller (FluidNCClient) is
    // responsible for actually sending the `$SD/ListJSON=/` command.
    void beginCapture();

    bool isCapturing() const { return capturing_; }

    // Bail out of an in-progress capture without producing a result (e.g.
    // on websocket disconnect mid-response).
    void abort() { capturing_ = false; buffer_ = ""; }

    // Feed one already-line-buffered, non-status line while capturing.
    // Recognizes the "ok"/"error:" terminator and finalizes the list.
    void feedLine(const char *line);

    // Call every loop iteration; aborts the capture if the terminator
    // never arrives (e.g. disconnect mid-response).
    void checkTimeout();

    bool ready() const { return ready_; }
    void clearReady() { ready_ = false; }
    int count() const { return count_; }
    const FluidNCFileEntry &entry(int i) const { return files_[i]; }

private:
    static const uint32_t TIMEOUT_MS = 5000;
    static const size_t MAX_BUFFER_BYTES = 8192;

    bool capturing_ = false;
    uint32_t requestedAt_ = 0;
    String buffer_;
    bool ready_ = false;
    FluidNCFileEntry files_[MAX_FILES];
    int count_ = 0;

    void parseJson();
};
