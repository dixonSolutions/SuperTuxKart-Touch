#!/usr/bin/env bash
# Install SuperTuxKart Touch Flatpak from a prebuilt engine binary (no flatpak-builder).
# Intended for tablet devices that already compiled engine/build/bin/supertuxkart.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_ID="io.github.dixonSolutions.SuperTuxKartTouch"
WORKDIR="${FLATPAK_PREBUILT_DIR:-$ROOT/build-flatpak-prebuilt}"
REPO="${WORKDIR}/repo"
BUILD="${WORKDIR}/build"
BRANCH="${FLATPAK_BRANCH:-master}"
RUNTIME_VER="${FLATPAK_RUNTIME_VERSION:-25.08}"

BIN="$ROOT/engine/build/bin/supertuxkart"
test -x "$BIN" || { echo "Build the engine first" >&2; exit 1; }

command -v flatpak >/dev/null || { echo "flatpak required" >&2; exit 1; }
need_runtime() {
  local ref="$1"
  flatpak info "$ref" >/dev/null 2>&1 && return 0
  # Prefer system flathub when both system+user remotes exist (avoids interactive prompt).
  if flatpak remotes --system 2>/dev/null | grep -q '^flathub'; then
    flatpak install -y --system flathub "$ref"
  else
    flatpak install -y --user flathub "$ref"
  fi
}
need_runtime "org.freedesktop.Platform//${RUNTIME_VER}"
need_runtime "org.freedesktop.Sdk//${RUNTIME_VER}"

rm -rf "$WORKDIR"
mkdir -p "$REPO" "$BUILD"

flatpak build-init "$BUILD" "$APP_ID" org.freedesktop.Sdk org.freedesktop.Platform "$RUNTIME_VER"

export DESTDIR="$BUILD/files"
bash "$ROOT/scripts/stage-flatpak.sh"

flatpak build-finish "$BUILD" \
  --command=start.sh \
  --share=network --share=ipc \
  --socket=wayland --socket=fallback-x11 --socket=pulseaudio \
  --device=dri \
  --filesystem=xdg-data/supertuxkart-touch:create \
  --filesystem=xdg-config/supertuxkart:create \
  --filesystem=/var/lib/flatpak/app/net.supertuxkart.SuperTuxKart:ro

flatpak build-export "$REPO" "$BUILD" "$BRANCH"

REMOTE_NAME="supertuxkart-touch-local"
flatpak --user remote-delete --force "$REMOTE_NAME" 2>/dev/null || true
flatpak --user remote-add --no-gpg-verify --if-not-exists "$REMOTE_NAME" "file://${REPO}"
flatpak --user uninstall -y "$APP_ID" 2>/dev/null || true
flatpak --user install -y "$REMOTE_NAME" "$APP_ID"

echo
echo "Installed $APP_ID"
echo "Run: flatpak run $APP_ID"
echo "Perf:  STK_TOUCH_PERF=thermal|balanced|quality flatpak run $APP_ID"
