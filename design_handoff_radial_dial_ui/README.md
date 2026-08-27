# Handoff: TerraPen Dial Controller UI (historical)

> **Status: superseded. The firmware is the source of truth, not this file.**
>
> This is the design brief the panel was originally built from, kept for
> provenance — it's why the UI looks the way it does, and the palette,
> typography and icon-set sections below are still accurate and still worth
> reading before adding a screen.
>
> Everything describing *behaviour* has been overtaken by the shipped
> firmware. Where the two disagree, the firmware wins. The known divergences,
> as of the rename to terraDial:
>
> - **Home is a ring of nine, not eight**, and in a different order: Home XY,
>   Jog, Pen, Jobs, Progress, E-Stop, Alarm, Lights, Settings. `DIAL_ITEMS` in
>   `src/display/ui_dial.cpp` is authoritative.
> - **Job Progress is a dial destination**, not only an automatic screen.
> - **Jobs and Settings are rings on an open arc**, not carousels.
> - **There is no separate physical touch button** on this hardware. Back is a
>   knob long-press; E-Stop is a dial item, not a double-press gesture.
> - **Homing does not unconditionally send `$X`.** It unlocks only when the
>   machine is actually alarmed, and holds the `$H` until that has landed —
>   sending both together alarms the machine on any home after the first. See
>   the README's note on pacing FluidNC commands.
> - **The job-progress screen has no "pen down" pill.** It was redundant and
>   collided with the back button. It gained elapsed time and an estimate of
>   time remaining instead.
> - **Text entry exists**: the radial keyboard, which this brief lists as not
>   yet designed.

## Overview
Screen mockups for the terraPen/terraForge companion controller: a round CrowPanel 1.28" ESP32 HMI display (240×240 IPS, capacitive touch + a rotary knob that also clicks, plus a separate physical touch button). Home is a radial dial (8 destinations arranged around the ring, spring-eased rotation on select). Jobs and Settings use a carousel pattern since those lists don't fit a ring as cleanly.

Grounded against the real `terraDial` repo (warderoid-ctrl/terraDial, then named terraTouch) — palette hex values, the 8 Home destinations and their order, and the Lights/Settings screen contents all match `include/palette.h`, `src/display/ui_dial.cpp`, `ui_lights.cpp`, and `ui_settings.cpp`. (At the time of writing, `ui_dial.cpp` still implemented Home as a carousel and this brief was asking for the radial dial that has since shipped.)

## About the Design Files
The bundled file (`TerraPen Dial UI.dc.html`) is a **design reference built in HTML/CSS**, viewable in any browser. It is NOT code to embed — the target runtime is almost certainly **LVGL on ESP32/TFT_eSPI** (per the `terrapen-LCD-UI` repo), so recreate these layouts using LVGL widgets (`lv_arc`, `lv_img`/`lv_canvas` for icons, `lv_label`, etc.) at the real **240×240px** resolution. If a different target stack is used instead, adapt to its existing environment/patterns rather than porting the HTML directly.

## Fidelity
High-fidelity for color, layout proportion, and icon choice. Pixel positions are given at the 440×440 mockup scale — divide by ~1.83 to map to the real 240×240 screen (or better: treat the proportions/percentages as source of truth, not literal px).

All screens were grounded in the repo as it stood when this was written; several have moved on since.

## Screens
1. **Home** — radial dial of 8 destinations: Jobs, Jog, Pen, Lights, Home (clear+$X), E-Stop, Alarm, Settings (order matches `ui_dial.cpp`'s `DIAL_ITEMS`). All 8 sit on a ring at a fixed radius from center; the one nearest the top slot is largest/brightest (accent fill), others shrink and fade continuously with angular distance from top. Rotating the knob spins the ring with a springy eased transition (~550ms, overshoot easing) instead of the old flat carousel swap; knob click or tap opens the item nearest the top.
2. **Jobs (SD card)** — carousel of files on the SD card. Center card shows file icon, filename truncated to one line with ellipsis (never wraps) + a second small line for the remainder, meta (point count, est. time), and a "Run job" pill. Long filenames are the normal case, not an edge case — design accordingly.
3. **Job in progress (Plotting)** — progress ring drawn by layering: outer conic-gradient ring (12px inset) + inner circle matching background (23px inset) leaves an ~11px arc band. Center: percentage (52px bold) + time remaining. Pause/stop 58px circular buttons below. "pen down" status pill near the bottom edge.
4. **Jog** — X/Y/Z axis chips at top (Y active in the mock). Big position readout center, ± 58px circular jog buttons either side. Increment chip row: 0.1 / 1 / 10 mm — **Z axis omits the 10mm chip**, only 0.1/1 available; called out in an on-screen caption.
5. **Pen up/down** — a segmented control (not a toggle switch), "Pen up" / "Pen down", accent fill on the active side. Tap either half or click the knob to flip.
6. **Lights** — brightness sliders for "Dial ring" and "terraPen LED" plus 4 colour-preset swatches for the dial ring (matches `ui_lights.cpp`).
7. **Home / clear prompt** — safety gate shown before homing: icon + "Clear the bed first" + body copy + "Confirm & home" pill. Confirming sends `$X` (unlock) then homes.
8. **E-Stop** — single always-reachable action. Big red circular button (a muted safety-red, `oklch(62% 0.19 25)`, to avoid a jarring pure red against the rest of the palette) with alert icon, "E-STOP" label, "Feed hold + soft reset" subtext. Should also be triggerable by double-pressing the physical touch button from any screen.
9. **Alarm clear** — shows the alarm reason (e.g. "Hard limit triggered · Z−"), not just a bare button. "Clear alarm" pill sends `$X`. Intended to appear automatically when an alarm trips.
10. **Settings** — carousel: Wi-Fi (SSID, password dots, Connect pill — shown as the main card), Machine, Display, About (matches `ui_settings.cpp`). On-screen keyboard for SSID/password entry not designed yet.

## Interactions & Behavior
- **Rotary knob rotation**: pages the active carousel (Home/Jobs/Settings), jogs the active axis (Jog), or adjusts the focused slider (Lights).
- **Rotary knob click**: confirms/opens the highlighted card; flips Pen up/down; confirms Home/Alarm-clear prompts.
- **Touchscreen tap**: direct-select any card/button/chip.
- **Physical touch button**: back/home from any screen; double-press = E-Stop from anywhere.
- No transition/animation timing was specified — recommend a quick (150–200ms) ease for card paging and state changes.
- Firmware commands referenced: `$X` (alarm unlock, used by both the Home-clear flow and Alarm-clear), homing after unlock on the Home flow. **Superseded** — see the divergence note at the top: unlocking unconditionally before `$H` is what made repeat homing alarm.

## Design Tokens
Mockup OKLCH values below match the real hex values already committed to `include/palette.h` — that file is the source of truth for firmware; these are the equivalent OKLCH mixes used to build this mockup in HTML.
- **Background (page canvas, not device)**: `oklch(96% 0.006 80)` warm off-white
- **Screen background**: radial-gradient `oklch(29% 0.025 290)` → `oklch(15% 0.02 290)` (deep graphite-lavender) — `Palette::bgApp() 0x201e29` / hub `Palette::bgTerminal() 0x322f3e`
- **Foreground/primary text**: `oklch(97% 0.005 90)`
- **Muted text/icons/border**: `oklch(60–70% 0.02 290)` — `Palette::border() 0x71707d`, `Palette::textMuted() 0x8f8d9b`
- **Accent (primary, selection/progress)**: `oklch(78% 0.09 300)` — lavender — `Palette::accent() 0xc0abe9`
- **Accent (secondary, network/pen-down/alarm)**: `oklch(80% 0.08 45)` — warm peach — `Palette::accentSecondary() 0xeaae93`
- **Accent foreground (text/icons on accent fills)**: `Palette::accentFg() 0x18141f`
- **Selected-row fill**: `Palette::selectedFill() 0x453a5e`
- **Alert (E-Stop only)**: `oklch(62% 0.19 25)` — muted safety red — `Palette::alert() 0xc4483f`
- **Typography**: Manrope (400/500/600/700/800) for UI text; JetBrains Mono (500/600) for small-caps labels/eyebrow text
- Icon set: **Lucide** (thin 2px stroke, rounded caps/joins) — icons used: layers, target, file, sliders-horizontal, activity, plus, play, clock, layout-grid, pause, square, wifi, sun, cpu, info, maximize (jog), pen-line, lightbulb, home, unlock, alert-circle

## Assets
No external image assets — all icons are inline Lucide SVGs; the file thumbnail is a placeholder striped pattern (real plot-preview thumbnails to be substituted).

## Files
- `TerraPen Dial UI.dc.html` — the full mockup set (open in a browser; view source for exact markup/CSS values).
