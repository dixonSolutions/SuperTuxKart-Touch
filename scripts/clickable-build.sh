#!/usr/bin/env bash
# Build Ubuntu Touch .click (invoked inside Clickable image or locally).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLICK_NAME="${CLICK_NAME:-$(cat "$ROOT/click/.ci-name" 2>/dev/null || echo supertuxkarttouch.dixonsolutions)}"
CLICK_VERSION="${CLICK_VERSION:-$(cat "$ROOT/click/.ci-version" 2>/dev/null || echo 1.0.0)}"
CLICK_FRAMEWORK="${CLICK_FRAMEWORK:-$(cat "$ROOT/click/.ci-framework" 2>/dev/null || echo ubuntu-touch-24.04-1.x)}"
ARCH="${ARCH:-${CLICKABLE_ARCH:-arm64}}"
case "$ARCH" in
  arm64|aarch64) CLICK_ARCH=arm64; TRIPLET=aarch64-linux-gnu ;;
  armhf) CLICK_ARCH=armhf; TRIPLET=arm-linux-gnueabihf ;;
  amd64|x86_64) CLICK_ARCH=amd64; TRIPLET=x86_64-linux-gnu ;;
  *) CLICK_ARCH=$ARCH; TRIPLET="${ARCH}-linux-gnu" ;;
esac

bash "$ROOT/scripts/build-engine.sh"

STAGE="$ROOT/build-click/${CLICK_NAME}"
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/share/icons" "$STAGE/data"

install -m 755 "$ROOT/engine/build/bin/supertuxkart" "$STAGE/bin/supertuxkart"
install -m 755 "$ROOT/packaging/start.sh" "$STAGE/bin/start.sh"
rsync -a --exclude 'tracks/' --exclude 'karts/' --exclude 'library/' \
  --exclude 'models/' --exclude 'music/' --exclude 'textures/' \
  "$ROOT/engine/data/" "$STAGE/data/"
cp -f "$ROOT/engine/data/supertuxkart_256.png" "$STAGE/share/icons/supertuxkart.png"

sed -e "s/@CLICK_NAME@/${CLICK_NAME}/g" \
    -e "s/@CLICK_VERSION@/${CLICK_VERSION}/g" \
    -e "s/@CLICK_FRAMEWORK@/${CLICK_FRAMEWORK}/g" \
    -e "s/@CLICK_ARCH@/${CLICK_ARCH}/g" \
    "$ROOT/click/manifest.json.in" > "$STAGE/manifest.json"
cp "$ROOT/click/supertuxkart.desktop" "$STAGE/"
cp "$ROOT/click/supertuxkart.apparmor" "$STAGE/"

# click package
if command -v click >/dev/null; then
  (cd "$(dirname "$STAGE")" && click build "$(basename "$STAGE")")
  find "$(dirname "$STAGE")" -maxdepth 1 -name '*.click' -exec mv {} "$ROOT/" \;
  echo "Built click in $ROOT"
else
  echo "click tool not found — staged tree at $STAGE (CI image provides click)" >&2
fi
