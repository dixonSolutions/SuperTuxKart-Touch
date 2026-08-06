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

1. **`TOUCH_STK` + `TOUCH_STK_MOBILE_ASSETS`** — tablet defaults and in-engine
   DownloadAssets wizard when tracks are missing (Flatpak and Click). Flathub
   SuperTuxKart is an optional reuse path in `start.sh`, not a hard requirement.
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

## Assets

The engine downloads race assets on first launch (`DownloadAssets`, `MOBILE_STK`),
so the package deliberately ships without the 141 MB of tracks. That split has one
hard constraint: **the wizard lives in the GUI, so the engine must finish booting
before the user can ever see it.** Shipping empty asset directories therefore does
not degrade gracefully — it kills the app on launch:

- `sfx/sfx.xml` missing → `SFXManager` logs `[fatal]` and exits.
- `textures/materials.xml` missing → `FileManager` logs `[fatal]` and exits.
- Zero karts leaves the menus without a valid default kart.

`scripts/fetch-boot-assets.py` closes that gap at build time. It pulls only the
boot-critical directories (`sfx`, `models`, `textures`, `library`, `karts`, ~87 MB)
out of the upstream `stk-assets-mobile` release using HTTP range requests, so a
build transfers a fraction of the 228 MB archive. `scripts/stage-flatpak.sh` runs
it, and the release tag is taken from the engine's `PROJECT_VERSION` — the same
value `STK_VERSION` puts in the wizard's download URL, so bundled and downloaded
assets always come from one release.

`packaging/start.sh` can instead borrow a full tree from a locally installed
Flathub `net.supertuxkart.SuperTuxKart` to skip the download. That is a shortcut
only: discovery returns *foreign* trees exclusively, and the merged runtime is
rebuilt from scratch each launch, because symlinks into a Flathub commit go stale
as soon as that app is updated or removed. Only the heavy media directories are
borrowed; GUI, skins and touch icons always come from our own package so the
stock tree cannot shadow the fork's UI.

The first-launch wizard is pushed when `!ExtractMobileAssets::hasFullAssets()`
and there are no **official** (non-addon) tracks. Addon tracks alone must not
suppress it. The dialog is queued with `from_queue=true` so `loadFromFile` runs
from `DialogQueue::update` — loading in the constructor breaks the queue
contract and segfaults during skin draw. When the download finishes, Touch
aborts the main loop and `execv`s `/proc/self/exe` so music and tracks load on
a clean relaunch (in-process `sameRestart` is not enough).

## Config

See `touch/default-multitouch.xml.snippet`. Flatpak users can enable the stock binary’s multitouch GUI with that block alone; the fork adds glass art + forced defaults.
