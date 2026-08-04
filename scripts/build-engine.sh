#!/usr/bin/env bash
# Configure and build TOUCH_STK engine (host or Flatpak SDK).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Inside flatpak-builder the host engine/build cache is invalid — use a clean dir.
if [ -n "${FLATPAK_ID:-}" ] || [ -d /run/build ]; then
  BUILD="${ROOT}/engine/cmake-build-flatpak"
  rm -rf "$BUILD"
elif [ -n "${STK_BUILD_DIR:-}" ]; then
  BUILD="$STK_BUILD_DIR"
  rm -rf "$BUILD"
else
  BUILD="$ROOT/engine/build"
fi
mkdir -p "$BUILD"
cd "$BUILD"
CMAKE_ARGS=(
  -DCMAKE_BUILD_TYPE=Release
  -DTOUCH_STK=ON
  -DCHECK_ASSETS=off
  -DNO_SHADERC=on
  -DBUILD_RECORDER=off
  -DCMAKE_INSTALL_PREFIX=/usr
)
# Clickable / cross: honour toolchain compilers when set.
if [ -n "${CC:-}" ]; then CMAKE_ARGS+=(-DCMAKE_C_COMPILER="$CC"); fi
if [ -n "${CXX:-}" ]; then CMAKE_ARGS+=(-DCMAKE_CXX_COMPILER="$CXX"); fi
cmake "$ROOT/engine" "${CMAKE_ARGS[@]}"
cmake --build . -j"$(nproc)"
test -x "$BUILD/bin/supertuxkart"
# Publish a stable path for staging scripts
mkdir -p "$ROOT/engine/build/bin"
cp -f "$BUILD/bin/supertuxkart" "$ROOT/engine/build/bin/supertuxkart"
echo "Built $BUILD/bin/supertuxkart"
