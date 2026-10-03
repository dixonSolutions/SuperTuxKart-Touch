# Ubuntu Touch launch contract

Ubuntu Touch runs click apps inside AppArmor confinement (`aa-exec`). Host
binaries outside the click tree are not executable (`dirname`, `mkdir`, `xrandr`,
…). This matches the Xonotic-Touch contract.

## Issue history

| Report | Symptom | Cause |
|--------|---------|--------|
| [#1](https://github.com/dixonSolutions/SuperTuxKart-Touch/issues/1) | `dirname: Permission denied`, `//bin/supertuxkart` | Launcher used `/usr/bin/dirname` |
| [#2](https://github.com/dixonSolutions/SuperTuxKart-Touch/issues/2) | Lomiri exit **126**; manual run: `game assets not found` | (1) host `mkdir` under `set -e` → 126; (2) Click detection required `manifest.json`, which is **not** in the installed data tree |
| [#3](https://github.com/dixonSolutions/SuperTuxKart-Touch/issues/3) | Logo, then crash; `SFXManager` fatal on `sfx.xml`; cannot create `~/.config/supertuxkart` | (1) Click staging created empty `sfx/` stubs and never ran `fetch-boot-assets.py`; (2) XDG paths used unconfined `supertuxkart` dirs denied by AppArmor |
| [#4](https://github.com/dixonSolutions/SuperTuxKart-Touch/issues/4) | 1.0.18 hangs on welcome logo; same `~/.config/supertuxkart` mkdir denials | Lomiri pre-sets `XDG_*` to `$HOME/.config` etc.; `${XDG_*: -click-id}` kept those, so AppArmor still denied writes and the download zip could not land under addons |
| [#5](https://github.com/dixonSolutions/SuperTuxKart-Touch/issues/5) | 1.0.19 journal: launch is clean through `GrandPrixManager`, then three `apparmor="DENIED" … org.freedesktop.ScreenSaver … Inhibit` and nothing further | (1) SDL inhibits blanking through an interface confinement denies, so the display slept mid-game; the `keep-display-on` policy group grants `com.canonical.Unity.Screen` instead. (2) The engine logged nothing between the file manager banner and the main menu, and `grep -i supertux` dropped what it did log |

## Detection

Installed clicks store the manifest at `.click/info/<name>.manifest`, not
`manifest.json`. The staged package also ships:

- `supertuxkart.apparmor`
- `.supertuxkart-touch-click` (sentinel)

`packaging/start.sh` treats any of those as Click and skips Flatpak asset discovery.

## Click launch path

1. Resolve `APP_ROOT` with builtins (`cd` + `$PWD`, `${var%/*}`) and `APP_DIR`.
2. Set `LD_LIBRARY_PATH` to `$APP_ROOT/lib` when present.
3. **Force** confined XDG (do not keep Lomiri's `$HOME/.config` defaults):
   - `XDG_CONFIG_HOME=$HOME/.config/<click-id>`
   - `XDG_DATA_HOME=$HOME/.local/share/<click-id>`
   - `XDG_CACHE_HOME=$HOME/.cache/<click-id>`
   - `STK_TOUCH_ASSETS_DIR=$XDG_DATA_HOME/stk-assets`
   - `SUPERTUXKART_SAVEDIR=$XDG_CONFIG_HOME/supertuxkart`
4. Export `STK_TOUCH_CLICK=1` (engine: keep the display on through
   `com.canonical.Unity.Screen`) and `SDL_VIDEO_ALLOW_SCREENSAVER=1` (SDL: stop
   retrying the `org.freedesktop.ScreenSaver` call AppArmor denies).
5. Set `SUPERTUXKART_DATADIR=$APP_ROOT` and `exec` the engine.
6. **Do not** call host `mkdir` / `xrandr` / `powerprofilesctl` (exit 126 under AA).
7. First-run tracks/karts come from in-engine **MOBILE_STK** download into
   `$XDG_DATA_HOME/stk-assets/` (libc `mkdir`, not coreutils).
   Flatpak uses the same wizard when Flathub STK is not installed.

## Keeping the display on

SDL asks `org.freedesktop.ScreenSaver` to inhibit blanking. Confinement denies
that, so the screen used to dim and sleep while the game ran (issue #5). The
`keep-display-on` policy group the manifest requests grants a different
interface, which `src/utils/ubuntu_touch_screen.cpp` calls instead:

| | |
|-|-|
| Bus | system |
| Name / path / interface | `com.canonical.Unity.Screen` / `/com/canonical/Unity/Screen` / `com.canonical.Unity.Screen` |
| Acquire | `keepDisplayOn()` → `int32` request id |
| Release | `removeDisplayOnRequest(int32)` |

`libdbus-1.so.3` is resolved with `dlopen` — SDL already loads it, and no other
platform needs the link. Off Click (`STK_TOUCH_CLICK` unset) both calls are
no-ops, because everywhere else SDL's own inhibit works and stays in charge.

## On-device checks

**Attach `stdout.log`, not a filtered journal.** The engine writes its complete
log to the path it prints on the first line of every launch:

```bash
tail -f ~/.config/supertuxkarttouch.dixonsolutions/supertuxkart/config-0.10/stdout.log
```

`journalctl -f | grep -i supertux` keeps only the lines that happen to contain a
path, which is how issue #5 arrived with the whole loading phase missing. Use it
for confinement denials, which come from `dbus-daemon` rather than the app:

```bash
journalctl -f | grep -iE 'supertux|apparmor'
ls /opt/click.ubuntu.com/supertuxkarttouch.dixonsolutions/current/
# expect: bin/start.sh, .supertuxkart-touch-click, supertuxkart.apparmor, lib/*.so*
```

A healthy Click launch logs confined paths, e.g.:

- `Click XDG_CONFIG_HOME=.../.config/supertuxkarttouch.dixonsolutions`
- `Creating directory '.../.config/supertuxkarttouch.dixonsolutions/supertuxkart/...'`
- not `.../.config/supertuxkart/config-0.10/` (AppArmor denied)

…and then walks the startup milestones the engine logs under the same
`supertuxkart-touch` tag the launcher uses, so a stuck launch says where it
stopped:

```
supertuxkart-touch: startup: graphics, GUI and asset managers ready
supertuxkart-touch: startup: loading materials, models and karts
supertuxkart-touch: startup: materials, models and karts loaded
supertuxkart-touch: startup: player data and challenges ready
supertuxkart-touch: startup: showing the main menu
supertuxkart-touch: startup: entering the main loop
```
