#!/usr/bin/env bash
# Stage Flatpak install tree (slim: binary + engine data/gui; tracks from Flathub).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${DESTDIR:-/app}"
BIN="$ROOT/engine/build/bin/supertuxkart"
APP_ID="io.github.dixonSolutions.SuperTuxKartTouch"

test -x "$BIN" || { echo "Missing $BIN — run scripts/build-engine.sh first" >&2; exit 1; }

mkdir -p "$DEST/bin" "$DEST/data" \
  "$DEST/share/applications" \
  "$DEST/share/metainfo" \
  "$DEST/share/icons/hicolor/128x128/apps" \
  "$DEST/share/icons/hicolor/256x256/apps" \
  "$DEST/share/icons/hicolor/512x512/apps"

install -m 755 "$BIN" "$DEST/bin/supertuxkart"
install -m 755 "$ROOT/packaging/start.sh" "$DEST/bin/start.sh"

# Slim data: gui (with glass), shaders, skins, po, ttf, sfx stubs from engine/data
# Exclude huge track/kart blobs — discovered from Flathub STK at runtime.
rsync -a --delete \
  --exclude 'tracks/' \
  --exclude 'karts/' \
  --exclude 'library/' \
  --exclude 'models/' \
  --exclude 'music/' \
  --exclude 'textures/' \
  --exclude '.git/' \
  "$ROOT/engine/data/" "$DEST/data/"

# Ensure glass icons present
mkdir -p "$DEST/data/gui/icons/android"
cp -f "$ROOT/engine/data/gui/icons/android"/glass_*.png "$DEST/data/gui/icons/android/" 2>/dev/null || true
# Version stamp for git builds
if [[ ! -f "$DEST/data/supertuxkart.git" ]]; then
  printf 'SuperTuxKart Touch\n' > "$DEST/data/supertuxkart.git"
fi

install -m 644 "$ROOT/flatpak/${APP_ID}.desktop" \
  "$DEST/share/applications/${APP_ID}.desktop"
install -m 644 "$ROOT/flatpak/${APP_ID}.metainfo.xml" \
  "$DEST/share/metainfo/${APP_ID}.metainfo.xml"

install -m 644 "$ROOT/engine/data/supertuxkart_128.png" \
  "$DEST/share/icons/hicolor/128x128/apps/${APP_ID}.png"
install -m 644 "$ROOT/engine/data/supertuxkart_256.png" \
  "$DEST/share/icons/hicolor/256x256/apps/${APP_ID}.png"
install -m 644 "$ROOT/engine/data/supertuxkart_512.png" \
  "$DEST/share/icons/hicolor/512x512/apps/${APP_ID}.png"

echo "Staged Flatpak tree to $DEST"
