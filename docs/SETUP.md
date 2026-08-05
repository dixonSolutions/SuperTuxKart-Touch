# SuperTuxKart Touch — Setup

## Architecture call

New project (`SuperTuxKart-Touch`), not shared runtime code with Xonotic-Touch. STK already has multitouch devices, race GUI, touch settings dialog, and screen keyboard — this port turns those on by default and restyles the race HUD as glass.

## Build flags

| Flag | Purpose |
|------|---------|
| `-DTOUCH_STK=ON` | Defines `TOUCH_STK` (not `MOBILE_STK` on Linux); glass HUD; touch defaults |
| `-DCHECK_ASSETS=off` | Allow compile without full `stk-assets` tree next to `engine/` |
| `-DNO_SHADERC=on` | Skip Vulkan shaderc dependency |
| `-DBUILD_RECORDER=off` | Skip openglrecorder |

## Runtime assets

Point at Flatpak data (or svn `stk-assets`):

```bash
# System or --user Flathub STK (tablets often use the user install):
FLATPAK_DATA=$(find \
  "$HOME/.local/share/flatpak/app/net.supertuxkart.SuperTuxKart" \
  /var/lib/flatpak/app/net.supertuxkart.SuperTuxKart \
  -path '*/files/share/supertuxkart/data/tracks' -type d 2>/dev/null | head -1 | xargs dirname)

# Prefer running from a prefix that shares that data tree, or symlink:
ln -sfn "$FLATPAK_DATA/.." ~/SuperTuxKart-Touch/share-supertuxkart
```

STK discovers `data/` via install layout / cwd. Practical approach on Ultramarine tablet:

```bash
cd ~/SuperTuxKart-Touch/engine/build/bin
# Copy or bind-mount flatpak share; simplest: install to a prefix
cmake --install . --prefix ~/SuperTuxKart-Touch/prefix
# Then merge/symlink flatpak karts/tracks into prefix/share/supertuxkart/data/
```

## Touch defaults (`main_touch.cpp`)

After config load:

- `multitouch_active = 2`
- `multitouch_draw_gui = true`
- `screen_keyboard_status = 1`
- steering-wheel mode if undefined
- auto-acceleration default on (first run)

## In-race controls

| Control | Mapping |
|---------|---------|
| Left glass stick | Steer X + accel/brake Y (`BUTTON_STEERING`) |
| Item | `BUTTON_FIRE` (keyboard Space equivalent) |
| Drift | `BUTTON_SKIDDING` |
| Nitro | `BUTTON_NITRO` |
| Look back | `BUTTON_LOOK_BACKWARDS` |
| Rescue / Pause | top glass buttons |

Settings: Options → Input → Touch Device (`MultitouchSettingsDialog`).

## Device under test

`gdr --dev=marinesurface` (Ultramarine Linux tablet, Wayland).

## Tablet run (after build)

On the tablet (deps + Flatpak STK assets already installed):

```bash
# From a machine with the source tree:
./scripts/deploy-and-build-tablet.sh borysthebear@100.125.7.103

# On the tablet:
~/SuperTuxKart-Touch/scripts/run-on-tablet.sh
```

`run-on-tablet.sh` copies Flatpak `gui`/`skins` into a writable runtime, overlays `glass_*.png`, sets `SUPERTUXKART_DATADIR`, and launches `engine/build/bin/supertuxkart`.

Verified on marinesurface: log line `RaceGUIMultitouch: Glass touch HUD enabled.`, in-race glass stick + action cluster, and STK screen keyboard on text fields.
