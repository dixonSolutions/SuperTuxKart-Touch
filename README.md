# SuperTuxTouch

Touch-first [SuperTuxKart](https://supertuxkart.net/) for Linux tablets and phones (**not** Flathub `net.supertuxkart.SuperTuxKart`). Glass virtual stick, drift / item / nitro buttons, touch settings, and an in-game screen keyboard. Defaults favour **thermal** headroom on fanless devices.

| | |
|---|---|
| **Name** | SuperTuxTouch |
| **Flatpak id** | `io.github.dixonSolutions.SuperTuxKartTouch` |
| **Flatpak remote** | `supertuxtouch` |
| **OpenStore / Click** | `supertuxkarttouch.dixonsolutions` |
| **Assets** | Install Flathub STK once for tracks/karts (stays separate) |

## Quick install

### Ubuntu Touch (OpenStore)

[![OpenStore](https://open-store.io/badges/en_US.svg)](https://open-store.io/app/supertuxkarttouch.dixonsolutions)

Or sideload:

```bash
wget https://dixonSolutions.github.io/SuperTuxKart-Touch/click/latest-arm64.click
pkcon install-local --allow-untrusted latest-arm64.click
```

First launch downloads full tracks/karts (~200 MB) over the network into
`~/.local/share/supertuxkart-touch/stk-assets/`.

### Linux Flatpak (remote)

```bash
# 1) Race assets once (stock STK on Flathub — required for tracks/karts)
flatpak install -y flathub net.supertuxkart.SuperTuxKart

# 2) SuperTuxTouch remote (name includes "touch" — does not replace Flathub STK)
flatpak remote-add --user --if-not-exists --no-gpg-verify supertuxtouch \
  https://dixonSolutions.github.io/SuperTuxKart-Touch/flatpak

# 3) Install + run
flatpak install --user -y supertuxtouch io.github.dixonSolutions.SuperTuxKartTouch
flatpak run io.github.dixonSolutions.SuperTuxKartTouch
```

Offline `.flatpak` bundles (same app id) are on every [GitHub Release](https://github.com/dixonSolutions/SuperTuxKart-Touch/releases/latest):

```bash
flatpak install --user SuperTuxKartTouch-*-x86_64.flatpak
# or: SuperTuxKartTouch-*-aarch64.flatpak
```

## If the app does not start

Almost always **missing Flathub assets** (especially when STK was installed with `--user` on a tablet):

```bash
flatpak install -y flathub net.supertuxkart.SuperTuxKart
flatpak update --user io.github.dixonSolutions.SuperTuxKartTouch
flatpak run io.github.dixonSolutions.SuperTuxKartTouch
```

Check the launcher message:

```bash
flatpak run io.github.dixonSolutions.SuperTuxKartTouch 2>&1 | tee /tmp/stk-touch.log
# Look for: "game assets not found"  → install Flathub STK
# Look for: "Couldn't initialise irrlicht" → display/GPU session issue
```

Launch from the app grid (graphical session), not a bare SSH shell without `WAYLAND_DISPLAY` / `DISPLAY`.

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
