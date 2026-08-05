#!/usr/bin/env bash
# Stage Ubuntu Touch click tree into INSTALL_DIR (Clickable packages it).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${DEST:-${INSTALL_DIR:?set DEST or INSTALL_DIR}}"
BIN="$ROOT/engine/build/bin/supertuxkart"
CLICK_NAME="${CLICK_NAME:-supertuxkarttouch.dixonsolutions}"
CLICK_VERSION="${CLICK_VERSION:-1.0.0}"
CLICK_FRAMEWORK="${CLICK_FRAMEWORK:-ubuntu-touch-24.04-1.x}"

resolve_click_arch() {
  case "${CLICK_ARCH:-${ARCH:-}}" in
    arm64|aarch64) printf 'arm64'; return ;;
    armhf|armv7*) printf 'armhf'; return ;;
    amd64|x86_64) printf 'amd64'; return ;;
  esac
  case "$(uname -m)" in
    aarch64|arm64) printf 'arm64' ;;
    armv7*|armhf) printf 'armhf' ;;
    *) printf 'amd64' ;;
  esac
}

copy_shared_libs() {
  local binary="$1" lib_dir="$2"
  local list seen_file lib base dest current
  list="$(mktemp)"
  seen_file="$(mktemp)"
  printf '%s\n' "$binary" > "$list"
  while [ -s "$list" ]; do
    current="$(head -n1 "$list")"
    sed -i '1d' "$list"
    grep -Fxq "$current" "$seen_file" 2>/dev/null && continue
    printf '%s\n' "$current" >> "$seen_file"
    while IFS= read -r lib; do
      case "$lib" in ''|linux-vdso.so.*) continue ;; esac
      [ -f "$lib" ] || continue
      base="$(basename "$lib")"
      case "$base" in
        ld-linux*.so*|libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libresolv.so*|libgcc_s.so*|libstdc++.so*)
          continue ;;
      esac
      dest="$lib_dir/$base"
      if [ ! -f "$dest" ]; then
        install -m 755 "$lib" "$dest"
      fi
      if ! grep -Fxq "$lib" "$seen_file" 2>/dev/null; then
        printf '%s\n' "$lib" >> "$list"
      fi
    done < <(ldd "$current" 2>/dev/null | awk '/=> \// {print $3}')
  done
  rm -f "$list" "$seen_file"
}

CLICK_ARCH="$(resolve_click_arch)"
test -x "$BIN" || { echo "Missing $BIN — run scripts/build-engine.sh first" >&2; exit 1; }

rm -rf "$DEST"
mkdir -p "$DEST/bin" "$DEST/lib" "$DEST/data" "$DEST/share/icons"

install -m 755 "$BIN" "$DEST/bin/supertuxkart"
install -m 755 "$ROOT/packaging/start.sh" "$DEST/bin/start.sh"

# Slim data (cp — no rsync in Clickable images)
EXCLUDE='tracks|karts|library|models|music|textures|\.git'
shopt -s nullglob dotglob
for entry in "$ROOT/engine/data"/*; do
  base="$(basename "$entry")"
  [[ "$base" =~ ^($EXCLUDE)$ ]] && continue
  if [[ -d "$entry" ]]; then
    mkdir -p "$DEST/data/$base"
    cp -a "$entry"/. "$DEST/data/$base/"
  else
    cp -a "$entry" "$DEST/data/"
  fi
done
shopt -u nullglob dotglob
mkdir -p "$DEST/data/gui/icons/android"
cp -f "$ROOT/engine/data/gui/icons/android"/glass_*.png "$DEST/data/gui/icons/android/" 2>/dev/null || true
printf 'SuperTuxKart Touch\n' > "$DEST/data/supertuxkart.git"

# MOBILE_STK discoverPaths requires these dirs in the package; real content
# comes from the in-engine download (~/.local/share/supertuxkart-touch/stk-assets/).
for stub in tracks karts library models music sfx textures; do
  mkdir -p "$DEST/data/$stub"
done

copy_shared_libs "$DEST/bin/supertuxkart" "$DEST/lib"

ICON_SRC="$ROOT/engine/data/supertuxkart_256.png"
install -m 644 "$ICON_SRC" "$DEST/supertuxkart.png"
install -m 644 "$ICON_SRC" "$DEST/share/icons/supertuxkart.png"

install -m 644 "$ROOT/click/supertuxkart.desktop" "$DEST/supertuxkart.desktop"

# click-review wants policy_version as a number (2404.1 for UT 24.04).
python3 - "$ROOT/click/supertuxkart.apparmor" "$DEST/supertuxkart.apparmor" "${APPARMOR_POLICY:-2404.1}" <<'PY'
import json, sys
src, dest, policy = sys.argv[1], sys.argv[2], sys.argv[3]
data = json.load(open(src))
data["policy_version"] = float(policy) if policy else float(data.get("policy_version", 2404.1))
data.pop("template", None)
data["policy_groups"] = [g for g in data.get("policy_groups", []) if g != "opengl"]
json.dump(data, open(dest, "w"), indent=4)
open(dest, "a").write("\n")
PY

sed \
  -e "s|@CLICK_NAME@|${CLICK_NAME}|g" \
  -e "s|@CLICK_VERSION@|${CLICK_VERSION}|g" \
  -e "s|@CLICK_ARCH@|${CLICK_ARCH}|g" \
  -e "s|@CLICK_FRAMEWORK@|${CLICK_FRAMEWORK}|g" \
  "$ROOT/click/manifest.json.in" > "$DEST/manifest.json"

echo "Staged click tree to $DEST (arch=${CLICK_ARCH}, version=${CLICK_VERSION})"
