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
4. Set `SUPERTUXKART_DATADIR=$APP_ROOT` and `exec` the engine.
5. **Do not** call host `mkdir` / `xrandr` / `powerprofilesctl` (exit 126 under AA).
6. First-run tracks/karts come from in-engine **MOBILE_STK** download into
   `$XDG_DATA_HOME/stk-assets/` (libc `mkdir`, not coreutils).
   Flatpak uses the same wizard when Flathub STK is not installed.

## On-device checks

```bash
journalctl -f | grep -i supertux
ls /opt/click.ubuntu.com/supertuxkarttouch.dixonsolutions/current/
# expect: bin/start.sh, .supertuxkart-touch-click, supertuxkart.apparmor, lib/*.so*

# Healthy Click launch logs should show confined paths, e.g.:
#   Click XDG_CONFIG_HOME=.../.config/supertuxkarttouch.dixonsolutions
#   Creating directory '.../.config/supertuxkarttouch.dixonsolutions/supertuxkart/...'
# Not: '.../.config/supertuxkart/config-0.10/' (AppArmor denied)
```
