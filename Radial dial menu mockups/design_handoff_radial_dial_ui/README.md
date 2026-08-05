# Handoff: TerraPen Dial Controller UI

## Overview
Screen mockups for the terraPen/terraForge companion controller: a round CrowPanel 1.28" ESP32 HMI display (240×240 IPS, capacitive touch + a rotary knob that also clicks, plus a separate physical touch button). All multi-item menus use a carousel pattern (not a radial ring) since Home alone has 8 destinations.

## About the Design Files
The bundled file (`TerraPen Dial UI.dc.html`) is a **design reference built in HTML/CSS**, viewable in any browser. It is NOT code to embed — the target runtime is almost certainly **LVGL on ESP32/TFT_eSPI** (per the `terrapen-LCD-UI` repo), so recreate these layouts using LVGL widgets (`lv_arc`, `lv_img`/`lv_canvas` for icons, `lv_label`, etc.) at the real **240×240px** resolution. If a different target stack is used instead, adapt to its existing environment/patterns rather than porting the HTML directly.

## Fidelity
High-fidelity for color, layout proportion, and icon choice. Pixel positions are given at the 440×440 mockup scale — divide by ~1.83 to map to the real 240×240 screen (or better: treat the proportions/percentages as source of truth, not literal px).

**Open gap:** the "Lights" screen and the extra Settings items (beyond Wi-Fi) are inferred, not sourced — the `terraTouch` repo referenced in chat was never actually shared into this project. Get that link and correct those two screens before building.

## Screens
1. **Home** — carousel of 8 destinations: Jobs, Jog, Pen, Lights, Home (clear+$X), E-Stop, Alarm Clear, Settings. Center card 190×190 (icon + label), dimmed neighbor cards peek at the edges, "n of 8" index text below. Knob rotation pages, knob click or tap opens.
2. **Jobs (SD card)** — carousel of files on the SD card. Center card shows file icon, filename truncated to one line with ellipsis (never wraps) + a second small line for the remainder, meta (point count, est. time), and a "Run job" pill. Long filenames are the normal case, not an edge case — design accordingly.
3. **Job in progress (Plotting)** — progress ring drawn by layering: outer conic-gradient ring (12px inset) + inner circle matching background (23px inset) leaves an ~11px arc band. Center: percentage (52px bold) + time remaining. Pause/stop 58px circular buttons below. "pen down" status pill near the bottom edge.
4. **Jog** — X/Y/Z axis chips at top (Y active in the mock). Big position readout center, ± 58px circular jog buttons either side. Increment chip row: 0.1 / 1 / 10 mm — **Z axis omits the 10mm chip**, only 0.1/1 available; called out in an on-screen caption.
5. **Pen up/down** — a segmented control (not a toggle switch), "Pen up" / "Pen down", accent fill on the active side. Tap either half or click the knob to flip.
6. **Lights** *(inferred — see gap above)* — brightness sliders for "Dial ring" and "terraPen LED" plus 4 colour-preset swatches for the dial ring.
7. **Home / clear prompt** — safety gate shown before homing: icon + "Clear the bed first" + body copy + "Confirm & home" pill. Confirming sends `$X` (unlock) then homes.
8. **E-Stop** — single always-reachable action. Big red circular button (a muted safety-red, `oklch(62% 0.19 25)`, to avoid a jarring pure red against the rest of the palette) with alert icon, "E-STOP" label, "Feed hold + soft reset" subtext. Should also be triggerable by double-pressing the physical touch button from any screen.
9. **Alarm clear** — shows the alarm reason (e.g. "Hard limit triggered · Z−"), not just a bare button. "Clear alarm" pill sends `$X`. Intended to appear automatically when an alarm trips.
10. **Settings** — carousel: Wi-Fi (SSID, password dots, Connect pill — shown as the main card), Machine, Display, About *(items 2–4 inferred — see gap above)*. On-screen keyboard for SSID/password entry not designed yet.

## Interactions & Behavior
- **Rotary knob rotation**: pages the active carousel (Home/Jobs/Settings), jogs the active axis (Jog), or adjusts the focused slider (Lights).
- **Rotary knob click**: confirms/opens the highlighted card; flips Pen up/down; confirms Home/Alarm-clear prompts.
- **Touchscreen tap**: direct-select any card/button/chip.
- **Physical touch button**: back/home from any screen; double-press = E-Stop from anywhere.
- No transition/animation timing was specified — recommend a quick (150–200ms) ease for card paging and state changes.
- Firmware commands referenced: `$X` (alarm unlock, used by both the Home-clear flow and Alarm-clear), homing after unlock on the Home flow.

## Design Tokens
- **Background (page canvas, not device)**: `oklch(96% 0.006 80)` warm off-white
- **Screen background**: radial-gradient `oklch(29% 0.025 290)` → `oklch(15% 0.02 290)` (deep graphite-lavender)
- **Foreground/primary text**: `oklch(97% 0.005 90)`
- **Muted text/icons**: `oklch(60–70% 0.02 290)`
- **Accent (primary, selection/progress)**: `oklch(78% 0.09 300)` — lavender
- **Accent (secondary, network/pen-down/alarm)**: `oklch(80% 0.08 45)` — warm peach
- **Alert (E-Stop only)**: `oklch(62% 0.19 25)` — muted safety red
- **Typography**: Manrope (400/500/600/700/800) for UI text; JetBrains Mono (500/600) for small-caps labels/eyebrow text
- Icon set: **Lucide** (thin 2px stroke, rounded caps/joins) — icons used: layers, target, file, sliders-horizontal, activity, plus, play, clock, layout-grid, pause, square, wifi, sun, cpu, info, maximize (jog), pen-line, lightbulb, home, unlock, alert-circle

## Assets
No external image assets — all icons are inline Lucide SVGs; the file thumbnail is a placeholder striped pattern (real plot-preview thumbnails to be substituted).

## Files
- `TerraPen Dial UI.dc.html` — the full mockup set (open in a browser; view source for exact markup/CSS values).
