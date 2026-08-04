#!/usr/bin/env bash
# Prepare runtime (Flatpak assets + glass overlays) and launch TOUCH_STK binary.
set -euo pipefail
ROOT="${HOME}/SuperTuxKart-Touch"
BIN="$ROOT/engine/build/bin/supertuxkart"
RUNTIME="$ROOT/runtime"

FP_DATA="$(find /var/lib/flatpak/app/net.supertuxkart.SuperTuxKart \
  -path '*/files/share/supertuxkart/data/tracks' -type d 2>/dev/null | head -1 | xargs dirname)"
[[ -n "$FP_DATA" ]] || { echo "Flatpak STK data not found"; exit 1; }
[[ -x "$BIN" ]] || { echo "Build first: cmake --build $ROOT/engine/build -j\$(nproc)"; exit 1; }

if [[ ! -f "$RUNTIME/data/supertuxkart.git" ]]; then
  rm -rf "$RUNTIME"
  mkdir -p "$RUNTIME/data"
  for item in "$FP_DATA"/*; do
    base="$(basename "$item")"
    case "$base" in
      gui|skins) cp -a "$item" "$RUNTIME/data/$base" ;;
      *) ln -sfn "$item" "$RUNTIME/data/$base" ;;
    esac
  done
  if [[ -f "$ROOT/engine/data/supertuxkart.git" ]]; then
    cp -f "$ROOT/engine/data/supertuxkart.git" "$RUNTIME/data/"
  else
    cp -fL "$FP_DATA"/supertuxkart.1.5 "$RUNTIME/data/supertuxkart.git" 2>/dev/null \
      || touch "$RUNTIME/data/supertuxkart.git"
  fi
fi

for d in "$RUNTIME/data/gui/icons/android" \
         "$RUNTIME/data/skins"/*/data/gui/icons/android; do
  [[ -d "$d" ]] || continue
  cp -f "$ROOT/engine/data/gui/icons/android"/glass_*.png "$d/" 2>/dev/null || true
done

pkill -f '[.]/supertuxkart' 2>/dev/null || true
sleep 0.5
export SUPERTUXKART_DATADIR="$RUNTIME"
export DISPLAY="${DISPLAY:-:0}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
cd "$(dirname "$BIN")"
exec env SUPERTUXKART_DATADIR="$RUNTIME" "$BIN" --screensize=2880x1920 "$@"
