#!/usr/bin/env bash
# Configure and build TOUCH_STK engine (host or Flatpak SDK).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Inside flatpak-builder the host engine/build cache is invalid — use a clean dir.
if [ -n "${FLATPAK_ID:-}" ] || [ -d /run/build ]; then
  BUILD="${ROOT}/engine/cmake-build-flatpak"
  rm -rf "$BUILD"
else
  BUILD="${STK_BUILD_DIR:-$ROOT/engine/build}"
fi
mkdir -p "$BUILD"
cd "$BUILD"
cmake "$ROOT/engine" \
  -DCMAKE_BUILD_TYPE=Release \
  -DTOUCH_STK=ON \
  -DCHECK_ASSETS=off \
  -DNO_SHADERC=on \
  -DBUILD_RECORDER=off \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build . -j"$(nproc)"
test -x "$BUILD/bin/supertuxkart"
# Publish a stable path for staging scripts
mkdir -p "$ROOT/engine/build/bin"
cp -f "$BUILD/bin/supertuxkart" "$ROOT/engine/build/bin/supertuxkart"
echo "Built $BUILD/bin/supertuxkart"
