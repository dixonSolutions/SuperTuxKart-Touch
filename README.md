# SuperTuxTouch

Touch-first [SuperTuxKart](https://supertuxkart.net/) for Linux tablets and phones (**not** Flathub `net.supertuxkart.SuperTuxKart`). Glass virtual stick, drift / item / nitro buttons, touch settings, and an in-game screen keyboard. Defaults favour **thermal** headroom on fanless devices.

| | |
|---|---|
| **Name** | SuperTuxTouch |
| **Flatpak id** | `io.github.dixonSolutions.SuperTuxKartTouch` |
| **Flatpak remote** | `supertuxtouch` |
| **OpenStore / Click** | `supertuxkarttouch.dixonsolutions` |
| **Assets** | First-launch download wizard (Flathub STK optional reuse) |

## Quick install

### Ubuntu Touch (OpenStore)

[![OpenStore](https://open-store.io/badges/en_US.svg)](https://open-store.io/app/supertuxkarttouch.dixonsolutions)

Or sideload:

```bash
wget https://dixonSolutions.github.io/SuperTuxKart-Touch/click/latest-arm64.click
pkcon install-local --allow-untrusted latest-arm64.click
```

First launch opens an in-game setup dialog that downloads tracks/karts (~200 MB) into
`~/.local/share/supertuxkart-touch/stk-assets/`.

### Linux Flatpak (remote)

```bash
flatpak remote-add --user --if-not-exists --no-gpg-verify supertuxtouch \
  https://dixonSolutions.github.io/SuperTuxKart-Touch/flatpak
flatpak install --user -y supertuxtouch io.github.dixonSolutions.SuperTuxKartTouch
flatpak run io.github.dixonSolutions.SuperTuxKartTouch
```

First launch shows the same asset download wizard if race data is missing.
Optional: install Flathub `net.supertuxkart.SuperTuxKart` to reuse its tracks/karts
instead of downloading (Touch stays a separate app).

Offline `.flatpak` bundles (same app id) are on every [GitHub Release](https://github.com/dixonSolutions/SuperTuxKart-Touch/releases/latest):

```bash
flatpak install --user SuperTuxKartTouch-*-x86_64.flatpak
# or: SuperTuxKartTouch-*-aarch64.flatpak
```

## If the app does not start

```bash
flatpak run io.github.dixonSolutions.SuperTuxKartTouch 2>&1 | tee /tmp/stk-touch.log
# Look for: "Couldn't initialise irrlicht" → display/GPU session issue
```

Launch from the app grid (graphical session), not a bare SSH shell without `WAYLAND_DISPLAY` / `DISPLAY`.
On first run, complete the in-game **Download** step (needs network).

## Performance

| Profile | Env | Notes |
|---------|-----|--------|
| **thermal** (default) | `STK_TOUCH_PERF=thermal` | 30 FPS, RTT 0.55, lights/shadows off |
| balanced | `STK_TOUCH_PERF=balanced` | 45 FPS, mid quality |
| quality | `STK_TOUCH_PERF=quality` | 60 FPS, richer graphics |

```bash
STK_TOUCH_PERF=balanced flatpak run io.github.dixonSolutions.SuperTuxKartTouch
```

## Maintainer workflow

```bash
./scripts/build-engine.sh
./scripts/install-flatpak.sh --from-remote --run
# or local builder:
./scripts/install-flatpak.sh --clean
```

| Path | Purpose |
|------|---------|
| `engine/` | stk-code fork (`TOUCH_STK`, glass HUD) |
| `packaging/start.sh` | Launcher: Flathub asset discover, glass overlay, perf |
| `flatpak/` | Manifest, desktop, metainfo |
| `click/` | Ubuntu Touch click metadata |
| `docs/` | Architecture, releases, media |

## Docs

- [docs/RELEASES.md](docs/RELEASES.md) — Flatpak + Click remotes, OpenStore
- [docs/SETUP.md](docs/SETUP.md) — build & tablet run
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — design decisions

## License

GPL-3 — same as SuperTuxKart upstream.
