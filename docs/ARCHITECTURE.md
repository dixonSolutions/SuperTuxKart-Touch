# SuperTuxKart Touch — Architecture

## Decision: new project

Xonotic-Touch is DarkPlaces + QuakeC. SuperTuxKart is Irrlicht/C++ with an upstream multitouch stack. Sharing game code would be artificial. This repo is a **new project** that forks `stk-code` under `engine/` and reuses only packaging/process ideas from Xonotic-Touch.

## What upstream already had

| Piece | Location |
|-------|----------|
| Touch device + actions | `src/input/multitouch_device.*` |
| Race HUD buttons | `src/states_screens/race_gui_multitouch.*` |
| Settings dialog | `src/states_screens/dialogs/multitouch_settings_dialog.*` |
| Screen keyboard | `src/guiengine/screen_keyboard.*` |

## What this port adds

1. **`TOUCH_STK` CMake option** — tablet-first defaults; Click builds also set
   `TOUCH_STK_MOBILE_ASSETS` → `MOBILE_STK` for first-run asset download.
2. **`main_touch.cpp`** — force multitouch GUI + screen keyboard after config load.
3. **Glass race HUD** — virtual stick (steer + accel/brake) and tinted glass plates for item/drift/nitro/look.
4. **Glass textures** — `data/gui/icons/android/glass_*.png`.
5. **First-run control picker** — `InitAndroidDialog` also for `TOUCH_STK`.

## Control mapping (race)

```
Left stick (BUTTON_STEERING)
  axis_x → steer L/R
  axis_y → accel / brake

Right cluster
  FIRE   → item (Space)
  SKID   → drift
  NITRO  → boost
  LOOK   → look back
```

## Config

See `touch/default-multitouch.xml.snippet`. Flatpak users can enable the stock binary’s multitouch GUI with that block alone; the fork adds glass art + forced defaults.
