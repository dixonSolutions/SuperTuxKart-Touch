#!/bin/sh
# Launch SuperTuxKart Touch (Flatpak, local, Click).
#
# Ubuntu Touch AppArmor denies exec of host coreutils (dirname, mkdir, xrandr, …).
# Until we know we are not confined, use shell builtins only — see
# Xonotic-Touch docs/UBUNTU_TOUCH_LAUNCH.md for the same contract.
set -e

stk_log() {
    echo "supertuxkart-touch: $*" >&2
}

# `cd` + $PWD (builtins) — never /usr/bin/pwd or dirname.
stk_resolve_dir() {
    CDPATH='' cd -P -- "$1" 2>/dev/null && echo "$PWD"
}

stk_parent_dir() {
    case "$1" in
        */*) echo "${1%/*}" ;;
        *) echo "." ;;
    esac
}

# Lomiri Exec=bin/start.sh → $0 is relative; APP_DIR is set by lomiri-app-launch.
APP_ROOT=""
for stk_root_candidate in \
    "${STK_TOUCH_APP_ROOT:-}" \
    "${APP_DIR:-}" \
    "$(stk_parent_dir "$0")/.." \
    .
do
    [ -n "$stk_root_candidate" ] || continue
    stk_root_resolved="$(stk_resolve_dir "$stk_root_candidate")" || continue
    if [ -n "$stk_root_resolved" ] && [ -x "$stk_root_resolved/bin/supertuxkart" ]; then
        APP_ROOT="$stk_root_resolved"
        break
    fi
done

if [ -z "$APP_ROOT" ]; then
    stk_log "binary not found (script=$0 APP_DIR=${APP_DIR:-unset})"
    exit 1
fi

BIN="${APP_ROOT}/bin/supertuxkart"
USER_BASE="${STK_TOUCH_USER_BASE:-${HOME}/.local/share/supertuxkart-touch}"
USER_DATA="${USER_BASE}/data"
RUNTIME_ROOT="${USER_BASE}/runtime"
PERF="${STK_TOUCH_PERF:-thermal}"

# Click packages may ship private libs next to the binary.
if [ -d "${APP_ROOT}/lib" ]; then
    if [ -n "${LD_LIBRARY_PATH:-}" ]; then
        export LD_LIBRARY_PATH="${APP_ROOT}/lib:${LD_LIBRARY_PATH}"
    else
        export LD_LIBRARY_PATH="${APP_ROOT}/lib"
    fi
fi

# Installed clicks do NOT keep manifest.json in the data tree — Clickable puts it
# in control, and the device stores it as .click/info/<name>.manifest.
# Rely on hooks / sentinel that actually ship in data.tar.gz.
is_click_package() {
    [ -f "${APP_ROOT}/.supertuxkart-touch-click" ] && return 0
    [ -f "${APP_ROOT}/supertuxkart.apparmor" ] && return 0
    [ -d "${APP_ROOT}/.click/info" ] && return 0
    return 1
}

has_stk_data() {
    [ -d "$1/data/tracks" ] && [ -d "$1/data/karts" ]
}

# True when tracks look like a real Flathub/full tree (not Click placeholders).
has_full_stk_data() {
    has_stk_data "$1" || return 1
    for d in "$1"/data/tracks/*; do
        [ -d "$d" ] || continue
        return 0
    done
    return 1
}

try_stk_prefix() {
    if has_full_stk_data "$1"; then
        echo "$1"
        return 0
    fi
    return 1
}

# Only ever returns a *foreign* full asset tree (Flathub). The packaged prefix is
# the default and needs no discovery, and RUNTIME_ROOT is a derived copy — treating
# either as a discovery result made the launcher symlink the slim package onto
# itself instead of falling through to the download wizard.
discover_stk_data() {
    for p in \
        "${HOME}/.local/share/flatpak/app/net.supertuxkart.SuperTuxKart/current/active/files/share/supertuxkart" \
        /var/lib/flatpak/app/net.supertuxkart.SuperTuxKart/current/active/files/share/supertuxkart
    do
        try_stk_prefix "$p" && return 0
    done

    for p in \
        "${HOME}/.local/share/flatpak/app/net.supertuxkart.SuperTuxKart"/*/active/files/share/supertuxkart \
        /var/lib/flatpak/app/net.supertuxkart.SuperTuxKart/*/active/files/share/supertuxkart \
        /var/lib/flatpak/app/net.supertuxkart.SuperTuxKart/*/stable/*/files/share/supertuxkart
    do
        try_stk_prefix "$p" && return 0
    done

    return 1
}

# Heavy media the fork never modifies, so it is safe to borrow from a full tree.
# Everything else must come from our own package: this fork ships custom GUI
# layouts, skins and touch icons that a stock Flathub tree would otherwise shadow.
BORROWABLE_ASSET_DIRS='tracks karts library models music sfx textures'

# Build a merged tree: our packaged data plus a foreign full asset tree, so the
# game gets real tracks/karts without a download while keeping our touch UI.
prepare_runtime() {
    SRC_DATA="$1/data"
    DEST="${RUNTIME_ROOT}"

    # Rebuild from scratch: symlinks from a previous run point into a Flathub
    # commit that an update or uninstall may since have removed.
    rm -rf "${DEST}/data"
    mkdir -p "${DEST}/data"

    for item in "${APP_ROOT}/data"/*; do
        [ -e "$item" ] || continue
        ln -sfn "$item" "${DEST}/data/${item##*/}"
    done

    for base in $BORROWABLE_ASSET_DIRS; do
        [ -d "${SRC_DATA}/$base" ] || continue
        ln -sfn "${SRC_DATA}/$base" "${DEST}/data/$base"
    done
}

# --- launch paths -------------------------------------------------------------

CLICK_PACKAGE=0
if is_click_package; then
    CLICK_PACKAGE=1
fi

if [ "$CLICK_PACKAGE" -eq 1 ]; then
    # Slim Click: in-engine MOBILE_STK downloads tracks/karts into
    # $XDG_DATA_HOME/stk-assets/ (libc mkdir, not /usr/bin/mkdir).
    STK_PREFIX="$APP_ROOT"
    stk_log "Click launch from $APP_ROOT"
    # Do not call host mkdir/xrandr/powerprofilesctl — AppArmor returns 126.
    #
    # Confinement only allows writes under the click package name. Point XDG_*
    # there before the engine creates ~/.config/supertuxkart etc. (issue #3).
    CLICK_ID="${CLICK_NAME:-supertuxkarttouch.dixonsolutions}"
    # Prefer APP_ID from lomiri-app-launch when present (name_app_version).
    case "${APP_ID:-}" in
        *_*) CLICK_ID="${APP_ID%%_*}" ;;
    esac
    export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-${HOME}/.config/${CLICK_ID}}"
    export XDG_DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share/${CLICK_ID}}"
    export XDG_CACHE_HOME="${XDG_CACHE_HOME:-${HOME}/.cache/${CLICK_ID}}"
    export STK_TOUCH_ASSETS_DIR="${STK_TOUCH_ASSETS_DIR:-${XDG_DATA_HOME}/stk-assets}"
else
    # Desktop / Flatpak: host coreutils are available.
    mkdir -p "$USER_DATA" "$RUNTIME_ROOT"

    if command -v powerprofilesctl >/dev/null 2>&1; then
        powerprofilesctl set power-saver >/dev/null 2>&1 || true
    elif command -v flatpak-spawn >/dev/null 2>&1; then
        flatpak-spawn --host powerprofilesctl set power-saver >/dev/null 2>&1 || true
    fi

    FLATHUB_PREFIX="$(discover_stk_data || true)"
    if [ -n "${FLATHUB_PREFIX:-}" ]; then
        # Optional fast path: merge a full Flathub tree over our packaged data.
        prepare_runtime "$FLATHUB_PREFIX"
        STK_PREFIX="$RUNTIME_ROOT"
        stk_log "reusing Flathub assets from $FLATHUB_PREFIX"
    else
        # No Flathub STK — launch slim package; MOBILE_STK shows the download wizard.
        STK_PREFIX="$APP_ROOT"
        stk_log "no local Flathub assets — first launch will download tracks/karts"
    fi
fi

SCREENSIZE="${STK_TOUCH_SCREENSIZE:-}"
if [ "$CLICK_PACKAGE" -eq 0 ] && [ -z "$SCREENSIZE" ] && command -v xrandr >/dev/null 2>&1; then
    SCREENSIZE="$(xrandr 2>/dev/null | awk '/\*/{print $1; exit}' || true)"
fi
EXTRA_ARGS=""
if [ -n "$SCREENSIZE" ]; then
    EXTRA_ARGS="--screensize=${SCREENSIZE}"
fi

export STK_TOUCH_PERF="$PERF"
# STK appends /data/ to this path
export SUPERTUXKART_DATADIR="$STK_PREFIX"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"

cd "${BIN%/*}"
# shellcheck disable=SC2086
exec "$BIN" $EXTRA_ARGS "$@"
