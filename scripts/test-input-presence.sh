#!/usr/bin/env bash
# Checks what the engine reads out of /proc/bus/input/devices (touchscreen,
# keyboard, mouse or touchpad, tablet-mode switch) and the touch-controls /
# screen-keyboard decisions built on it, against recorded listings. No engine
# build, no display. See docs/TOUCH_DETECTION.md.
set -euo pipefail

ROOT="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
CXX="${CXX:-c++}"
dir="$ROOT/tests/input-presence"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

"$CXX" -std=c++11 -Wall -Wextra -Werror -I"$ROOT/engine/src" \
    -o "$out/input_presence_test" "$dir/input_presence_test.cpp"
"$out/input_presence_test" "$dir/fixtures"
