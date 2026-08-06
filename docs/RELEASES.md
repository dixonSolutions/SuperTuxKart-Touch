# Releases

Distribution model matches Xonotic-Touch: Flatpak OSTree remote + offline bundles on GitHub Releases + Ubuntu Touch `.click` / OpenStore.

| Channel | Value |
|---------|-------|
| Display name | **SuperTuxTouch** |
| Flatpak app id | `io.github.dixonSolutions.SuperTuxKartTouch` |
| Flatpak remote name | **`supertuxtouch`** (touch keyword — not Flathub) |
| Flatpak remote URL | https://dixonSolutions.github.io/SuperTuxKart-Touch/flatpak |
| Stock STK (assets only) | Flathub `net.supertuxkart.SuperTuxKart` |
| Click id | `supertuxkarttouch.dixonsolutions` |
| Click latest | https://dixonSolutions.github.io/SuperTuxKart-Touch/click/latest-arm64.click |
| OpenStore | https://open-store.io/app/supertuxkarttouch.dixonsolutions |

## Quick install

[![OpenStore](https://open-store.io/badges/en_US.svg)](https://open-store.io/app/supertuxkarttouch.dixonsolutions)

```bash
# Flatpak remote (Linux tablets)
flatpak install -y flathub net.supertuxkart.SuperTuxKart   # assets — required
flatpak remote-add --user --if-not-exists --no-gpg-verify supertuxtouch \
  https://dixonSolutions.github.io/SuperTuxKart-Touch/flatpak
flatpak install --user -y supertuxtouch io.github.dixonSolutions.SuperTuxKartTouch
flatpak run io.github.dixonSolutions.SuperTuxKartTouch
```

Remote name `supertuxtouch` and app id `…SuperTuxKartTouch` stay separate from Flathub `net.supertuxkart.SuperTuxKart`.

**Won’t start?** Install Flathub STK (system or `--user`). User installs need the Touch Flatpak that allows `xdg-data/flatpak/app/net.supertuxkart.SuperTuxKart` (1.0.9+).

## CI

`Build and publish` builds Flatpak natively — `ubuntu-24.04` for x86_64 and
`ubuntu-24.04-arm` for aarch64 (no QEMU). Click/OpenStore jobs do not wait on Flatpak.

## Flatpak offline (GitHub Release)

Each release attaches:

- `SuperTuxKartTouch-<ver>-x86_64.flatpak`
- `SuperTuxKartTouch-<ver>-aarch64.flatpak`
- arm64/armhf `.click` packages

```bash
flatpak install --user SuperTuxKartTouch-*-x86_64.flatpak
```

## Performance

Default `STK_TOUCH_PERF=thermal` (30 FPS, RTT scale 0.55, lights/shadows off).

```bash
STK_TOUCH_PERF=balanced flatpak run io.github.dixonSolutions.SuperTuxKartTouch
STK_TOUCH_PERF=quality  flatpak run io.github.dixonSolutions.SuperTuxKartTouch
```

## Click / OpenStore

CI builds arm64 + armhf `.click` on each `main` push and uploads revisions when `OPENSTORE_API_KEY` is set.

Slim Click packages use the upstream **MOBILE_STK** asset download on first launch
(`stk-assets.zip` → `~/.local/share/supertuxkart-touch/stk-assets/`). The launcher
uses shell builtins only under AppArmor (no host `dirname` / `mkdir` / `xrandr`) and
detects Click via `supertuxkart.apparmor` / `.supertuxkart-touch-click` (installed
trees do not keep `manifest.json` in the data dir). See
[UBUNTU_TOUCH_LAUNCH.md](UBUNTU_TOUCH_LAUNCH.md).

| | |
|--|--|
| Manage | https://open-store.io/manage/supertuxkarttouch.dixonsolutions |
| Public | https://open-store.io/app/supertuxkarttouch.dixonsolutions |
