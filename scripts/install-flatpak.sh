#!/usr/bin/env bash
# Build (flatpak-builder) or install SuperTuxKart Touch Flatpak.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_ID="${FLATPAK_APP_ID:-io.github.dixonSolutions.SuperTuxKartTouch}"
REMOTE_NAME="${FLATPAK_REMOTE_NAME:-supertuxtouch}"
REMOTE_URL="${FLATPAK_REMOTE_URL:-https://dixonSolutions.github.io/SuperTuxKart-Touch/flatpak}"
MANIFEST="${FLATPAK_MANIFEST:-flatpak/io.github.dixonSolutions.SuperTuxKartTouch.yml}"
BUILD_DIR="${FLATPAK_BUILD_DIR:-build-flatpak}"

FROM_REMOTE=0
ADD_REMOTE=0
PREBUILT=0
RUN_APP=0
CLEAN=0

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]
  --from-remote   Install from GitHub Pages remote
  --add-remote    Add public remote
  --prebuilt      Use scripts/install-flatpak-prebuilt.sh (no flatpak-builder)
  --run           Run after install
  --clean         Remove build dir first
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --from-remote) FROM_REMOTE=1; shift ;;
    --add-remote) ADD_REMOTE=1; shift ;;
    --prebuilt) PREBUILT=1; shift ;;
    --run) RUN_APP=1; shift ;;
    --clean) CLEAN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown: $1" >&2; usage; exit 1 ;;
  esac
done

if [ "$ADD_REMOTE" = 1 ]; then
  flatpak --user remote-add --if-not-exists --no-gpg-verify "$REMOTE_NAME" "$REMOTE_URL" \
    || flatpak --user remote-modify --url="$REMOTE_URL" "$REMOTE_NAME" || true
fi

if [ "$FROM_REMOTE" = 1 ]; then
  flatpak --user install -y "$REMOTE_NAME" "$APP_ID"
elif [ "$PREBUILT" = 1 ]; then
  bash "$ROOT/scripts/install-flatpak-prebuilt.sh"
else
  command -v flatpak-builder >/dev/null || {
    echo "flatpak-builder missing; try --prebuilt or install flatpak-builder" >&2
    exit 1
  }
  cd "$ROOT"
  [ "$CLEAN" = 1 ] && rm -rf "$BUILD_DIR"
  flatpak-builder --user --install --force-clean "$BUILD_DIR" "$MANIFEST"
fi

if [ "$RUN_APP" = 1 ]; then
  exec flatpak run "$APP_ID"
fi
echo "Installed. Run: flatpak run $APP_ID"
