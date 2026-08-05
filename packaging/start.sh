#!/bin/sh
# Launch SuperTuxKart Touch (Flatpak, local, Click).
# Avoid /usr/bin/dirname and /usr/bin/basename — AppArmor denies them on Ubuntu Touch.
set -e

# Resolve install root using shell parameter expansion only.
script=$0
case $script in
    /*) ;;
    *)
        # Lomiri Exec=bin/start.sh → cwd is the click package root.
        if [ -x "./bin/supertuxkart" ]; then
            script="$(pwd)/bin/start.sh"
        else
            script="$(pwd)/$script"
        fi
        ;;
esac
script_dir=${script%/*}
case $script_dir in
    */bin) APP_ROOT=${script_dir%/*} ;;
    *) APP_ROOT=$(CDPATH= cd -- "$script_dir/.." && pwd) ;;
esac
# Prefer absolute path when possible (pwd is a shell builtin).
APP_ROOT=$(CDPATH= cd -- "$APP_ROOT" && pwd)

BIN="${APP_ROOT}/bin/supertuxkart"
USER_BASE="${STK_TOUCH_USER_BASE:-${HOME}/.local/share/supertuxkart-touch}"
USER_DATA="${USER_BASE}/data"
RUNTIME_ROOT="${USER_BASE}/runtime"
PERF="${STK_TOUCH_PERF:-thermal}"

# Click packages ship private libs next to the binary.
if [ -d "${APP_ROOT}/lib" ]; then
    if [ -n "${LD_LIBRARY_PATH:-}" ]; then
        export LD_LIBRARY_PATH="${APP_ROOT}/lib:${LD_LIBRARY_PATH}"
    else
        export LD_LIBRARY_PATH="${APP_ROOT}/lib"
    fi
fi

if [ ! -x "$BIN" ]; then
    echo "supertuxkart-touch: binary not found at $BIN" >&2
    exit 1
fi

mkdir -p "$USER_DATA" "$RUNTIME_ROOT"

# Prefer host power-saver on tablets (best-effort; often unavailable in sandbox).
if command -v powerprofilesctl >/dev/null 2>&1; then
    powerprofilesctl set power-saver >/dev/null 2>&1 || true
elif command -v flatpak-spawn >/dev/null 2>&1; then
    flatpak-spawn --host powerprofilesctl set power-saver >/dev/null 2>&1 || true
fi

has_stk_data() {
    [ -d "$1/data/tracks" ] && [ -d "$1/data/karts" ]
}

# True when tracks look like a real Flathub/full tree (not Click placeholders).
has_full_stk_data() {
    has_stk_data "$1" || return 1
    # Placeholder dirs from stage-click are empty; Flathub has real track folders.
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

discover_stk_data() {
    # 1) Bundled full tree under /app or install prefix
    if [ -f "${APP_ROOT}/data/supertuxkart.git" ] || [ -f "${APP_ROOT}/data/supertuxkart.1.5" ]; then
        try_stk_prefix "${APP_ROOT}" && return 0
    fi

    # 2) Flathub SuperTuxKart — user then system (tablets often use --user).
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

    # 3) Previously prepared user runtime
    try_stk_prefix "${RUNTIME_ROOT}" && return 0
    return 1
}

prepare_runtime() {
    SRC_PREFIX="$1"
    SRC_DATA="${SRC_PREFIX}/data"
    DEST="${RUNTIME_ROOT}"
    mkdir -p "${DEST}/data"

    # Symlink heavy assets; copy writable gui/skins for glass overlays.
    for item in "$SRC_DATA"/*; do
        [ -e "$item" ] || continue
        base=${item##*/}
        case "$base" in
            gui|skins)
                rm -rf "${DEST}/data/$base"
                cp -a "$item" "${DEST}/data/$base"
                ;;
            *)
                ln -sfn "$item" "${DEST}/data/$base"
                ;;
        esac
    done

    if [ -f "${APP_ROOT}/data/supertuxkart.git" ]; then
        cp -f "${APP_ROOT}/data/supertuxkart.git" "${DEST}/data/"
    elif [ ! -e "${DEST}/data/supertuxkart.git" ]; then
        if [ -e "${DEST}/data/supertuxkart.1.5" ]; then
            cp -fL "${DEST}/data/supertuxkart.1.5" "${DEST}/data/supertuxkart.git" 2>/dev/null \
                || touch "${DEST}/data/supertuxkart.git"
        else
            touch "${DEST}/data/supertuxkart.git"
        fi
    fi

    if [ -d "${APP_ROOT}/data/gui/icons/android" ]; then
        for d in "${DEST}/data/gui/icons/android" \
                 "${DEST}/data/skins"/*/data/gui/icons/android; do
            [ -d "$d" ] || continue
            cp -f "${APP_ROOT}/data/gui/icons/android"/glass_*.png "$d/" 2>/dev/null || true
        done
    fi
}

# Ubuntu Touch Click: slim package + in-engine MOBILE_STK asset download.
is_click_package() {
    [ -f "${APP_ROOT}/manifest.json" ] && [ -d "${APP_ROOT}/lib" ]
}

if is_click_package; then
    STK_PREFIX="$APP_ROOT"
    echo "supertuxkart-touch: Click launch from $APP_ROOT" >&2
else
    STK_PREFIX="$(discover_stk_data || true)"
    if [ -z "${STK_PREFIX:-}" ]; then
        echo "supertuxkart-touch: game assets not found — app cannot start." >&2
        echo "Install Flathub SuperTuxKart once (system or --user), then retry:" >&2
        echo "  flatpak install -y flathub net.supertuxkart.SuperTuxKart" >&2
        echo "Or place a full data tree under ${APP_ROOT}/data" >&2
        exit 1
    fi

    case "$STK_PREFIX" in
        "$RUNTIME_ROOT") ;;
        *)
            prepare_runtime "$STK_PREFIX"
            STK_PREFIX="$RUNTIME_ROOT"
            ;;
    esac
fi

# Screen size hint for tablets (override with STK_TOUCH_SCREENSIZE)
SCREENSIZE="${STK_TOUCH_SCREENSIZE:-}"
if [ -z "$SCREENSIZE" ] && command -v xrandr >/dev/null 2>&1; then
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

BIN_DIR=${BIN%/*}
cd "$BIN_DIR"
# shellcheck disable=SC2086
exec "$BIN" $EXTRA_ARGS "$@"
