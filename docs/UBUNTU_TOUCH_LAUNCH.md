# Ubuntu Touch launch contract

Ubuntu Touch runs click apps inside AppArmor confinement (`aa-exec`). Host
binaries outside the click tree are not executable (`dirname`, `mkdir`, `xrandr`,
…). This matches the Xonotic-Touch contract.

## Issue history

| Report | Symptom | Cause |
|--------|---------|--------|
| [#1](https://github.com/dixonSolutions/SuperTuxKart-Touch/issues/1) | `dirname: Permission denied`, `//bin/supertuxkart` | Launcher used `/usr/bin/dirname` |
| [#2](https://github.com/dixonSolutions/SuperTuxKart-Touch/issues/2) | Lomiri exit **126**; manual run: `game assets not found` | (1) host `mkdir` under `set -e` → 126; (2) Click detection required `manifest.json`, which is **not** in the installed data tree |

## Detection

Installed clicks store the manifest at `.click/info/<name>.manifest`, not
`manifest.json`. The staged package also ships:

- `supertuxkart.apparmor`
- `.supertuxkart-touch-click` (sentinel)

`packaging/start.sh` treats any of those as Click and skips Flatpak asset discovery.

## Click launch path

1. Resolve `APP_ROOT` with builtins (`cd` + `$PWD`, `${var%/*}`) and `APP_DIR`.
2. Set `LD_LIBRARY_PATH` to `$APP_ROOT/lib` when present.
3. Set `SUPERTUXKART_DATADIR=$APP_ROOT` and `exec` the engine.
4. **Do not** call host `mkdir` / `xrandr` / `powerprofilesctl` (exit 126 under AA).
5. First-run tracks/karts come from in-engine **MOBILE_STK** download into
   `~/.local/share/supertuxkart-touch/stk-assets/` (libc `mkdir`, not coreutils).
   Flatpak uses the same wizard when Flathub STK is not installed.

## On-device checks

```bash
journalctl -f | grep -i supertux
ls /opt/click.ubuntu.com/supertuxkarttouch.dixonsolutions/current/
# expect: bin/start.sh, .supertuxkart-touch-click, supertuxkart.apparmor, lib/*.so*
```
