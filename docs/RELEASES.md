# Releases

Same distribution model as Xonotic-Touch.

| Channel | URL / ID |
|---------|----------|
| Flatpak app id | `io.github.dixonSolutions.SuperTuxKartTouch` |
| Flatpak remote | `https://dixonSolutions.github.io/SuperTuxKart-Touch/flatpak` |
| Click id | `supertuxkarttouch.dixonsolutions` |
| Click latest | `https://dixonSolutions.github.io/SuperTuxKart-Touch/click/latest-arm64.click` |
| OpenStore | app id `supertuxkarttouch.dixonsolutions` (needs `OPENSTORE_API_KEY` secret) |

## Flatpak install

```bash
# Once: Flathub STK for tracks/karts assets
flatpak install -y flathub net.supertuxkart.SuperTuxKart

flatpak remote-add --user --if-not-exists --no-gpg-verify supertuxkart-touch \
  https://dixonSolutions.github.io/SuperTuxKart-Touch/flatpak
flatpak install --user supertuxkart-touch io.github.dixonSolutions.SuperTuxKartTouch
flatpak run io.github.dixonSolutions.SuperTuxKartTouch
```

Local tablet (prebuilt binary, no CI wait):

```bash
./scripts/install-flatpak.sh --prebuilt --run
```

## Performance

Default `STK_TOUCH_PERF=thermal` (30 FPS, RTT scale 0.55, lights/shadows off).

```bash
STK_TOUCH_PERF=balanced flatpak run io.github.dixonSolutions.SuperTuxKartTouch
STK_TOUCH_PERF=quality  flatpak run io.github.dixonSolutions.SuperTuxKartTouch
```

## Click / OpenStore

CI builds arm64 + armhf `.click` on each `main` push. OpenStore upload uses repo secret `OPENSTORE_API_KEY` (same key as Xonotic-Touch).

**One-time:** create the listing at [open-store.io/submit](https://open-store.io/submit) with id exactly `supertuxkarttouch.dixonsolutions`, then mark **Published = Yes** after the first CI revision passes review.

| | |
|--|--|
| Manage | https://open-store.io/manage/supertuxkarttouch.dixonsolutions |
| Public | https://open-store.io/app/supertuxkarttouch.dixonsolutions |
