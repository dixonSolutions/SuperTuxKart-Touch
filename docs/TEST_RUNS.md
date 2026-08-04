# Test runs

## 2026-08-04 — Ultramarine tablet (`marinesurface`)

Device: Fedora/Ultramarine tablet via Tailscale `100.125.7.103` (GDR `marinesurface`).

### Flatpak (SDK build) — PASS

Built on-device with `flatpak-builder` against Freedesktop **25.08** (host binary packaging failed earlier on `GLIBC_2.43`).

```bash
flatpak-builder --user --install --force-clean build-flatpak \
  flatpak/io.github.dixonSolutions.SuperTuxKartTouch.yml
# Launch inside the graphical session:
eval "$(systemctl --user show-environment | grep -E '^(DISPLAY|WAYLAND_DISPLAY|XDG_RUNTIME_DIR)=')"
flatpak run io.github.dixonSolutions.SuperTuxKartTouch
```

Verified:

| Check | Result |
|-------|--------|
| App id / window | `io.github.dixonSolutions.SuperTuxKartTouch` / “SuperTuxKart Touch” |
| MainTouch thermal | Log: `MainTouch: Touch defaults applied … perf=thermal` |
| Glass HUD | Log: `RaceGUIMultitouch: Glass touch HUD enabled.` |
| Assets | Flathub `net.supertuxkart.SuperTuxKart` + glass overlay runtime |
| CPU in race | ~**17%** (was revving before thermal profile) |
| Power profile | `power-saver` via launcher |

Media: `docs/media/main-menu.png`, `docs/media/glass-hud-race.png`.

### Click / OpenStore / Pages remote

Scaffolded like Xonotic-Touch. CI publishes Flatpak OSTree + `.click` remotes on GitHub Pages after the first green `build-and-publish` run. OpenStore needs:

1. Create app id `supertuxkarttouch.dixonsolutions` on open-store.io
2. Repo secret `OPENSTORE_API_KEY`
