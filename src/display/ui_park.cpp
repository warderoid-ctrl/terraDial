#include "ui_park.h"
#include "../net/fluidnc_client.h"
#include "machine_extents.h"
#include "palette.h"
#include "ui_pen.h"
#include "ui_screen_shell.h"
#include <stdio.h>

// Built to the same shape as ui_home.cpp and ui_alarm_clear.cpp (icon ->
// title -> body -> single action pill), since like those it exists only to
// confirm something that moves the machine. What it adds is a status line,
// because unlike those this one takes half a minute and does three
// different things on the way.
namespace
{
    // Where the gantry ends up. Y only: homing has already put X at its own
    // zero, and the point of the move is to get the gantry off the drawing,
    // which is a Y question on this machine.
    const float PARK_FEED_MM_MIN = 3000.0f;

    // -- why this is a state machine and not three enqueued lines --
    //
    // FluidNC's command channel is a pipe, not a queue of intentions: $H
    // takes over the machine for as long as homing takes, and anything that
    // arrives during it is answered with error:9 rather than held. So each
    // step has to wait for evidence in the status stream that the previous
    // one finished, which means ticking, which means a state machine.
    enum class Phase : uint8_t
    {
        Off,         // not running
        LiftSettle,  // pen-lift jog issued, waiting for the machine to go quiet
        AwaitHoming, // $H issued, waiting to SEE Homing start
        AwaitIdle,   // homing under way, waiting for it to finish
        Travelling,  // park move issued
        Failed,
    };

    Phase phase = Phase::Off;
    uint32_t phaseAt = 0;

    // Homing has to be observed starting before we can wait for it to end.
    // Without this the sequence would look at the Idle the machine is
    // sitting in BEFORE homing begins, call that "homing finished", and send
    // the park move into a machine that hasn't moved yet.
    //
    // The window is generous because FluidNCClient::home() may itself sit on
    // a deferred $H for up to UNLOCK_TIMEOUT_MS (2s) when the machine needed
    // an unlock first.
    const uint32_t LIFT_SETTLE_MS = 500;
    const uint32_t LIFT_TIMEOUT_MS = 8000;
    const uint32_t HOMING_START_TIMEOUT_MS = 10000;
    // A 420mm axis at a typical homing seek rate, plus the pull-off and
    // re-approach, plus the second axis. Long enough not to give up on a slow
    // machine; short enough that a wedged sequence eventually says so.
    const uint32_t HOMING_DONE_TIMEOUT_MS = 180000;

    lv_obj_t *statusLbl = nullptr;

    void setPhase(Phase p)
    {
        phase = p;
        phaseAt = millis();
    }

    void setStatus(const char *s)
    {
        if (statusLbl) lv_label_set_text(statusLbl, s);
    }

    void fail(const char *why)
    {
        setPhase(Phase::Failed);
        setStatus(why);
    }

    void sendParkMove()
    {
        // G53 = "this line is in machine coordinates", so the move is
        // relative to the homing switches rather than to whatever work
        // offset happens to be active -- the whole point is a repeatable
        // physical position, and a G54 that was zeroed mid-bed would send the
        // gantry somewhere else entirely.
        //
        // G90 is stated rather than assumed: G53 needs absolute distance
        // mode, and nothing here has established it. FluidNC powers up in
        // G90, but a job that left G91 set behind it would otherwise turn
        // this into a 420mm RELATIVE move into the end stop.
        char cmd[48];
        snprintf(cmd, sizeof(cmd), "G90 G53 G0 Y%.3f F%.0f",
                 (double)MACHINE_Y_MAX_MM, (double)PARK_FEED_MM_MIN);
        fluidNC.sendGcodeLine(cmd);
    }

    void confirmBtnCb(lv_event_t *e) { (void)e; uiParkTrigger(); }
}

lv_obj_t *uiParkCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    // Same load-bearing vertical rhythm as ui_home.cpp -- see the note there.
    // This screen carries one extra line (the status), which is why the body
    // copy here is a single line where Home's is two.
    lv_obj_t *iconLbl = lv_label_create(scr);
    lv_label_set_text(iconLbl, LV_SYMBOL_IMAGE); // matches this item's dial icon
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(iconLbl, Palette::accent(), 0);
    lv_obj_align(iconLbl, LV_ALIGN_CENTER, 0, -58);

    lv_obj_t *titleLbl = lv_label_create(scr);
    lv_label_set_text(titleLbl, "Park for photo");
    lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(titleLbl, lv_color_white(), 0);
    lv_obj_align(titleLbl, LV_ALIGN_CENTER, 0, -28);

    lv_obj_t *bodyLbl = lv_label_create(scr);
    lv_label_set_text(bodyLbl, "Pen up, home, then out to Y max.");
    lv_obj_set_style_text_align(bodyLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(bodyLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bodyLbl, Palette::textMuted(), 0);
    lv_obj_align(bodyLbl, LV_ALIGN_CENTER, 0, -6);

    // The sequence outlasts this screen and is slow enough to look hung.
    // Without a running commentary, "homing" and "wedged" are the same
    // picture: a machine not visibly doing anything.
    statusLbl = lv_label_create(scr);
    lv_label_set_text(statusLbl, "");
    lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(statusLbl, Palette::accent(), 0);
    lv_obj_align(statusLbl, LV_ALIGN_CENTER, 0, 14);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 160, 38);
    lv_obj_set_style_bg_color(btn, Palette::accent(), 0);
    lv_obj_set_style_radius(btn, 19, 0);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -64);
    lv_obj_add_event_cb(btn, confirmBtnCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btnLbl = lv_label_create(btn);
    lv_label_set_text(btnLbl, "Confirm & park");
    lv_obj_set_style_text_font(btnLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(btnLbl, Palette::accentFg(), 0);
    lv_obj_center(btnLbl);

    addBackButton(scr);

    return scr;
}

void uiParkTrigger()
{
    // Re-tapping mid-sequence must not restart it -- a second $H part-way
    // through the first one is how you alarm the machine.
    if (phase != Phase::Off && phase != Phase::Failed) return;

    const FluidNCStatus &st = fluidNC.status();

    if (!st.connected)
    {
        fail("Not connected");
        return;
    }

    // The one genuinely destructive way to use this. Homing mid-plot drags
    // the carriage off the drawing and abandons the job, and the macro's
    // whole reason to exist is the moment AFTER a plot finishes -- so the
    // failure mode is someone reaching for it a little early. Refuse rather
    // than sequence it: there is no recovery once $H has started.
    if (st.jobActive || st.mode == MachineMode::Run || st.mode == MachineMode::Hold)
    {
        fail("Machine busy");
        return;
    }

    // Lift before homing, not after: homing drags the carriage the length of
    // both axes, and a pen left down draws a line across the finished plot on
    // the way. uiPenToggle() rather than a raw Z jog so the Pen screen's
    // segmented control still shows the truth afterwards.
    //
    // Skipped when alarmed, because an alarmed machine rejects jogs outright
    // -- the lift would be swallowed and we would sit waiting for motion that
    // was never going to happen. home() unlocks before it homes anyway, and a
    // pen that is already clear of the bed is the common case after a plot.
    bool alarmed = (st.mode == MachineMode::Alarm);
    if (!alarmed && uiPenIsDown()) uiPenToggle();

    setPhase(Phase::LiftSettle);
    setStatus(alarmed ? "Unlocking..." : "Lifting pen...");
}

void uiParkUpdate()
{
    if (phase == Phase::Off || phase == Phase::Failed) return;

    MachineMode mode = fluidNC.status().mode;
    uint32_t elapsed = millis() - phaseAt;

    // NOTE: there is deliberately no blanket "abort if Alarm" here. Alarm is
    // the state a freshly powered FluidNC sits in, and it is where a machine
    // lands after an E-Stop -- i.e. two of the most likely states to be in
    // when you reach for this. Aborting on it would refuse the sequence
    // exactly when it is most wanted, and homing is the cure for an alarm,
    // not a casualty of it. The alarm that DOES matter is one raised during
    // the homing cycle itself, which is checked in AwaitIdle below.

    switch (phase)
    {
        case Phase::LiftSettle:
            // Both conditions, not either: the settle time alone can expire
            // while the Z jog is still running, and Idle alone is true for
            // the moment before the jog we just queued has even reached the
            // machine (commands cross to networkTask through a queue).
            if (elapsed >= LIFT_SETTLE_MS &&
                (mode == MachineMode::Idle || mode == MachineMode::Alarm))
            {
                fluidNC.home();
                setPhase(Phase::AwaitHoming);
                setStatus("Homing...");
            }
            else if (elapsed >= LIFT_TIMEOUT_MS)
            {
                // Never went quiet. Something else is driving the machine
                // (terraForge, the web UI); homing into that is not ours to
                // do.
                fail("Machine didn't settle");
            }
            break;

        case Phase::AwaitHoming:
            if (mode == MachineMode::Homing) setPhase(Phase::AwaitIdle);
            else if (elapsed >= HOMING_START_TIMEOUT_MS) fail("Homing didn't start");
            break;

        case Phase::AwaitIdle:
            // An alarm now is a failed homing cycle -- a switch not reached,
            // or a soft limit tripped. The machine's coordinates are no
            // longer meaningful, so a G53 park move would be a full-speed
            // rapid to an arbitrary place.
            if (mode == MachineMode::Alarm)
            {
                fail("Homing failed");
            }
            else if (mode == MachineMode::Idle || mode == MachineMode::Done)
            {
                sendParkMove();
                setPhase(Phase::Travelling);
                setStatus("Parking...");
            }
            else if (elapsed >= HOMING_DONE_TIMEOUT_MS)
            {
                fail("Homing timed out");
            }
            break;

        case Phase::Travelling:
            // Done as soon as the move is away. Waiting for the machine to
            // arrive would mean another Run->Idle watch for no benefit --
            // nothing follows this step, and the dial's hub shows the live
            // state anyway.
            //
            // Deliberately does NOT navigate anywhere. The other confirm
            // screens bounce you to the dial, but they complete in the same
            // breath as the tap; this one finishes up to three minutes later,
            // by which time you may well be reading the Lights page. Yanking
            // the screen away then would be a change nobody asked for at a
            // moment nobody was watching.
            setPhase(Phase::Off);
            setStatus("Parked");
            break;

        default:
            break;
    }
}
