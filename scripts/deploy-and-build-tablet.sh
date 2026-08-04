#!/usr/bin/env bash
# Sync engine to marinesurface and build TOUCH_STK there.
set -euo pipefail
HOST="${1:-borysthebear@100.125.7.103}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

ssh "$HOST" 'mkdir -p ~/SuperTuxKart-Touch/engine'
rsync -az --exclude build --exclude .git --exclude cmake_build \
  "$ROOT/engine/" "$HOST:~/SuperTuxKart-Touch/engine/"

ssh "$HOST" 'bash -s' <<'EOF'
set -euo pipefail
cd ~/SuperTuxKart-Touch/engine
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DTOUCH_STK=ON \
  -DCHECK_ASSETS=off -DNO_SHADERC=on -DBUILD_RECORDER=off
cmake --build . -j"$(nproc)"
EOF

echo "Build finished on $HOST"
