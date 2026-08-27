#include "ui_job_progress.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include <stdio.h>
#include <string.h>

// Elapsed time is exact. The time-remaining estimate is derived here rather
// than reported by the machine -- see the ETA note below for where it comes
// from and why it's the shape it is. (This file used to carry no ETA at all,
// on the grounds that FluidNC gives a percentage and nothing else. That was
// the right call for the mockup's flat "~12 min left", which really did have
// no source; a trailing-rate estimate does have one.)
namespace
{
    lv_obj_t *ring = nullptr;
    lv_obj_t *filenameLbl = nullptr;
    lv_obj_t *percentLbl = nullptr;
    lv_obj_t *timingLbl = nullptr;
    lv_obj_t *pauseLbl = nullptr;

    // -- elapsed / ETA --
    //
    // FluidNC's SD:<pct> is the fraction of the file's BYTES consumed, not
    // work done, and G-code byte density is nowhere near uniform: a dense
    // hatch fill is thousands of byte-heavy short segments that plot slowly,
    // while one long G1 is a handful of bytes that takes a minute. So the
    // raw percentage races ahead through sparse regions and stalls in dense
    // ones. That's a property of the source -- no display treatment makes it
    // linear, which is why the number itself is left exactly as reported.
    //
    // The estimate therefore extrapolates from the RECENT rate (percent per
    // second over a trailing ~60s window) rather than the whole-job average.
    // It goes wrong for a minute or so after the drawing changes character
    // and then re-converges, which beats a figure that is confidently wrong
    // from start to finish. It stays prefixed with "~" to say so.
    const uint32_t SAMPLE_INTERVAL_MS = 2000;
    const int SAMPLE_COUNT = 30; // x 2s = a 60s trailing window

    struct ProgressSample
    {
        uint32_t ms;
        float pct;
    };
    ProgressSample samples[SAMPLE_COUNT];
    int sampleHead = 0;   // next slot to write
    int sampleCount = 0;  // slots filled, capped at SAMPLE_COUNT
    uint32_t lastSampleAt = 0;

    uint32_t jobStartedAt = 0;
    bool jobWasActive = false;
    char trackedFile[sizeof(FluidNCStatus::jobFilename)] = {0};

    void resetTracking(const FluidNCStatus &st)
    {
        sampleHead = 0;
        sampleCount = 0;
        lastSampleAt = 0;
        jobStartedAt = millis();
        snprintf(trackedFile, sizeof(trackedFile), "%s", st.jobFilename);
    }

    // Minutes remaining, or -1 while there isn't enough evidence to say.
    //
    // Both guards matter: without a real span of time the window is too
    // short to average out the percentage's own coarse granularity, and
    // without real movement across it the divisor approaches zero and the
    // estimate flies off to hours. Showing nothing is better than showing
    // a number that swings by an order of magnitude between refreshes.
    float etaMinutes(float pct)
    {
        if (sampleCount < 2) return -1.0f;
        const ProgressSample &oldest = samples[(sampleHead - sampleCount + SAMPLE_COUNT) % SAMPLE_COUNT];
        uint32_t spanMs = millis() - oldest.ms;
        float gained = pct - oldest.pct;
        if (spanMs < 20000 || gained <= 0.5f) return -1.0f;

        float remaining = 100.0f - pct;
        if (remaining <= 0.0f) return 0.0f;
        return (remaining / gained) * (spanMs / 60000.0f);
    }

    void formatElapsed(char *buf, size_t n, uint32_t ms)
    {
        uint32_t secs = ms / 1000;
        if (secs >= 3600)
            snprintf(buf, n, "%lu:%02lu:%02lu", (unsigned long)(secs / 3600),
                     (unsigned long)((secs / 60) % 60), (unsigned long)(secs % 60));
        else
            snprintf(buf, n, "%lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
    }

    void pauseBtnCb(lv_event_t *e) { (void)e; uiJobProgressTogglePause(); }

    void stopBtnCb(lv_event_t *e)
    {
        (void)e;
        // FluidNC has no separate "abort job cleanly" primitive -- same
        // feed-hold + soft-reset combo as E-Stop (ui_estop.cpp).
        fluidNC.feedHold();
        fluidNC.softReset();
    }
}

lv_obj_t *uiJobProgressCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    ring = lv_arc_create(scr);
    lv_obj_set_size(ring, 224, 224);
    lv_obj_center(ring);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_rotation(ring, 270); // start at 12 o'clock
    lv_arc_set_range(ring, 0, 100);
    lv_arc_set_value(ring, 0);
    lv_obj_set_style_arc_color(ring, Palette::accent(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ring, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ring, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, Palette::bgPanel(), LV_PART_MAIN);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    filenameLbl = lv_label_create(scr);
    lv_obj_set_style_text_font(filenameLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(filenameLbl, Palette::textMuted(), 0);
    lv_obj_set_width(filenameLbl, 160);
    lv_label_set_long_mode(filenameLbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(filenameLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(filenameLbl, LV_ALIGN_CENTER, 0, -56);

    // Sits ABOVE the percentage, not below it: below is where the pause and
    // stop buttons are (y +15..+73), and a line long enough to hold both the
    // clock and the estimate is wide enough to run into both of them.
    timingLbl = lv_label_create(scr);
    lv_obj_set_style_text_font(timingLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(timingLbl, Palette::textMuted(), 0);
    lv_obj_align(timingLbl, LV_ALIGN_CENTER, 0, -40);

    percentLbl = lv_label_create(scr);
    lv_obj_set_style_text_font(percentLbl, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(percentLbl, lv_color_white(), 0);
    lv_obj_align(percentLbl, LV_ALIGN_CENTER, 0, -14);

    lv_obj_t *pauseBtn = lv_btn_create(scr);
    lv_obj_set_size(pauseBtn, 58, 58);
    lv_obj_set_style_radius(pauseBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pauseBtn, Palette::bgSecondary(), 0);
    lv_obj_align(pauseBtn, LV_ALIGN_CENTER, -34, 44);
    lv_obj_add_event_cb(pauseBtn, pauseBtnCb, LV_EVENT_CLICKED, NULL);
    pauseLbl = lv_label_create(pauseBtn);
    lv_label_set_text(pauseLbl, LV_SYMBOL_PAUSE);
    lv_obj_center(pauseLbl);

    lv_obj_t *stopBtn = lv_btn_create(scr);
    lv_obj_set_size(stopBtn, 58, 58);
    lv_obj_set_style_radius(stopBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(stopBtn, Palette::alert(), 0); // same feedHold+softReset as E-Stop, so same colour
    lv_obj_align(stopBtn, LV_ALIGN_CENTER, 34, 44);
    lv_obj_add_event_cb(stopBtn, stopBtnCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *stopLbl = lv_label_create(stopBtn);
    lv_label_set_text(stopLbl, LV_SYMBOL_STOP);
    lv_obj_set_style_text_color(stopLbl, Palette::accentFg(), 0);
    lv_obj_center(stopLbl);

    addBackButton(scr);

    return scr;
}

void uiJobProgressUpdate(const FluidNCStatus &st)
{
    if (!ring) return;

    int pct = st.jobPercent >= 0 ? (int)st.jobPercent : 0;
    lv_arc_set_value(ring, pct);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(percentLbl, buf);

    // Restart the clock on a new job -- either the job flag going up, or the
    // filename changing under us (back-to-back runs can do that without
    // jobActive ever dropping between them).
    if (st.jobActive && (!jobWasActive || strncmp(trackedFile, st.jobFilename, sizeof(trackedFile)) != 0))
        resetTracking(st);
    jobWasActive = st.jobActive;

    if (!st.jobActive)
    {
        lv_label_set_text(timingLbl, "");
    }
    else
    {
        // Sample only while actually cutting. A pause would otherwise feed
        // the window a stretch of time with no progress in it, dragging the
        // computed rate toward zero and the estimate toward infinity -- the
        // ETA would balloon while the machine sat still and then take a full
        // window to recover after resuming.
        if (st.mode == MachineMode::Run && st.jobPercent >= 0 &&
            (lastSampleAt == 0 || millis() - lastSampleAt >= SAMPLE_INTERVAL_MS))
        {
            lastSampleAt = millis();
            samples[sampleHead].ms = lastSampleAt;
            samples[sampleHead].pct = st.jobPercent;
            sampleHead = (sampleHead + 1) % SAMPLE_COUNT;
            if (sampleCount < SAMPLE_COUNT) sampleCount++;
        }

        char elapsed[16];
        formatElapsed(elapsed, sizeof(elapsed), millis() - jobStartedAt);

        char line[64];
        float eta = etaMinutes(st.jobPercent);
        if (st.mode == MachineMode::Hold)      snprintf(line, sizeof(line), "%s   paused", elapsed);
        else if (eta < 0.0f || eta > 24 * 60)  snprintf(line, sizeof(line), "%s", elapsed);
        else if (eta < 1.0f)                   snprintf(line, sizeof(line), "%s   <1 min left", elapsed);
        else if (eta < 60.0f)                  snprintf(line, sizeof(line), "%s   ~%d min left", elapsed, (int)(eta + 0.5f));
        else                                   snprintf(line, sizeof(line), "%s   ~%dh %02dm left", elapsed, (int)eta / 60, (int)eta % 60);
        lv_label_set_text(timingLbl, line);
    }

    lv_label_set_text(filenameLbl, st.jobFilename[0] ? st.jobFilename : "--");
    lv_label_set_text(pauseLbl, st.mode == MachineMode::Hold ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
}

void uiJobProgressTogglePause()
{
    if (fluidNC.status().mode == MachineMode::Hold) fluidNC.resume();
    else fluidNC.feedHold();
}
