#!/usr/bin/env bash
# Launch SuperTuxKart Touch binary with Flatpak (or local) assets.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${STK_BIN:-$ROOT/engine/build/bin/supertuxkart}"
if [[ ! -x "$BIN" ]]; then
  BIN="$ROOT/engine/build/supertuxkart"
fi
if [[ ! -x "$BIN" ]]; then
  echo "Binary not found. Build with scripts/deploy-and-build-tablet.sh first." >&2
  exit 1
fi

FP_DATA="$(find /var/lib/flatpak/app/net.supertuxkart.SuperTuxKart \
  -path '*/files/share/supertuxkart/data/tracks' -type d 2>/dev/null | head -1 | xargs -r dirname || true)"

if [[ -n "${FP_DATA:-}" ]]; then
  export SUPERTUXKART_DATADIR="$(dirname "$FP_DATA")"
  # STK expects share/supertuxkart layout; Flatpak files/share/supertuxkart is the prefix.
  export SUPERTUXKART_DATADIR="$(dirname "$FP_DATA")"
fi

# Prefer XDG config under a touch-specific name when present
CFG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
mkdir -p "$CFG_HOME/supertuxkart/config-0.10"

exec "$BIN" "$@"
