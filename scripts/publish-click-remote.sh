#!/bin/bash
# Stage Ubuntu Touch .click packages onto the GitHub Pages site.
set -euo pipefail

ROOT="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
SITE_CLICK="${SITE_CLICK:-$ROOT/site/click}"
CLICK_REMOTE_URL="${CLICK_REMOTE_URL:-https://dixonSolutions.github.io/SuperTuxKart-Touch/click}"
PACKAGE_VERSION="${PACKAGE_VERSION:-}"
CLICK_NAME="${CLICK_NAME:-supertuxkarttouch.dixonsolutions}"
ARM64_CLICK="${ARM64_CLICK:-}"
ARMHF_CLICK="${ARMHF_CLICK:-}"

while [ $# -gt 0 ]; do
    case "$1" in
        --arm64) ARM64_CLICK="$2"; shift 2 ;;
        --armhf) ARMHF_CLICK="$2"; shift 2 ;;
        --version) PACKAGE_VERSION="$2"; shift 2 ;;
        --site) SITE_CLICK="$2"; shift 2 ;;
        -h|--help) echo "Usage: $0 --arm64 FILE --armhf FILE [--version VER]"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

test -n "$ARM64_CLICK" && test -f "$ARM64_CLICK" || { echo "Missing arm64 .click" >&2; exit 1; }
test -n "$ARMHF_CLICK" && test -f "$ARMHF_CLICK" || { echo "Missing armhf .click" >&2; exit 1; }
test -n "$PACKAGE_VERSION" || PACKAGE_VERSION="unknown"

mkdir -p "$SITE_CLICK"
install -m 644 "$ARM64_CLICK" "$SITE_CLICK/${CLICK_NAME}_${PACKAGE_VERSION}_arm64.click"
install -m 644 "$ARMHF_CLICK" "$SITE_CLICK/${CLICK_NAME}_${PACKAGE_VERSION}_armhf.click"
install -m 644 "$ARM64_CLICK" "$SITE_CLICK/latest-arm64.click"
install -m 644 "$ARMHF_CLICK" "$SITE_CLICK/latest-armhf.click"

cat > "$SITE_CLICK/latest.json" <<EOF
{
  "name": "${CLICK_NAME}",
  "version": "${PACKAGE_VERSION}",
  "framework": "ubuntu-touch-24.04-1.x",
  "packages": {
    "arm64": "${CLICK_REMOTE_URL}/latest-arm64.click",
    "armhf": "${CLICK_REMOTE_URL}/latest-armhf.click"
  }
}
EOF

cat > "$SITE_CLICK/index.html" <<EOF
<!DOCTYPE html>
<html lang="en">
<head><meta charset="utf-8"><title>SuperTuxTouch — Ubuntu Touch (.click)</title></head>
<body>
<h1>SuperTuxTouch Click packages</h1>
<p>App id <code>${CLICK_NAME}</code> — version ${PACKAGE_VERSION}</p>
<ul>
  <li><a href="latest-arm64.click">latest-arm64.click</a></li>
  <li><a href="latest-armhf.click">latest-armhf.click</a></li>
</ul>
</body>
</html>
EOF

printf 'Click remote staged at %s\n' "$SITE_CLICK"
