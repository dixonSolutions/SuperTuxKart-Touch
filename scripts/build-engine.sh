#!/usr/bin/env bash
# Configure and build TOUCH_STK engine (host, Flatpak SDK, or Clickable).
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
  -DUSE_WIIUSE=off
  -DCMAKE_INSTALL_PREFIX=/usr
)

# Ubuntu Touch / Clickable: GLES2 renderer (no desktop GL).
if [ -n "${INSTALL_DIR:-}" ] || [ "${STK_USE_GLES2:-}" = "1" ]; then
  CMAKE_ARGS+=(-DUSE_GLES2=on)
fi
# Flatpak + Click: in-engine DownloadAssets wizard when full tracks/karts are missing
# (Flathub SuperTuxKart is optional reuse, not required — same idea as Xonotic Touch).
CMAKE_ARGS+=(-DTOUCH_STK_MOBILE_ASSETS=ON)

# Clickable / cross: honour toolchain compilers when set.
if [ -n "${CC:-}" ]; then CMAKE_ARGS+=(-DCMAKE_C_COMPILER="$CC"); fi
if [ -n "${CXX:-}" ]; then CMAKE_ARGS+=(-DCMAKE_CXX_COMPILER="$CXX"); fi

# Cross builds: tell CMake the *target* CPU so AngelScript picks ARM/ARM64
# callfunc sources (host CMAKE_SYSTEM_PROCESSOR is often still x86_64).
case "${ARCH_TRIPLET:-${ARCH:-}}" in
  aarch64-*|arm64)
    CMAKE_ARGS+=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64)
    ;;
  arm-linux-gnueabihf|armhf)
    CMAKE_ARGS+=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm)
    ;;
esac

cmake "$ROOT/engine" "${CMAKE_ARGS[@]}"
cmake --build . -j"$(nproc)"
test -x "$BUILD/bin/supertuxkart"
# Publish a stable path for staging scripts
mkdir -p "$ROOT/engine/build/bin"
cp -f "$BUILD/bin/supertuxkart" "$ROOT/engine/build/bin/supertuxkart"
echo "Built $BUILD/bin/supertuxkart"
