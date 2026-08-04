#!/bin/sh
# Launch SuperTuxKart Touch (Flatpak, local, Click).
set -e

APP_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${APP_ROOT}/bin/supertuxkart"
USER_BASE="${STK_TOUCH_USER_BASE:-${HOME}/.local/share/supertuxkart-touch}"
USER_DATA="${USER_BASE}/data"
RUNTIME_ROOT="${USER_BASE}/runtime"
PERF="${STK_TOUCH_PERF:-thermal}"

if [ ! -x "$BIN" ]; then
    echo "supertuxkart-touch: binary not found at $BIN" >&2
    exit 1
fi

mkdir -p "$USER_DATA" "$RUNTIME_ROOT"

# Prefer host power-saver on tablets (best-effort).
if command -v powerprofilesctl >/dev/null 2>&1; then
    powerprofilesctl set power-saver >/dev/null 2>&1 || true
fi

discover_stk_data() {
    # 1) Bundled /app data (full or slim)
    if [ -f "${APP_ROOT}/data/supertuxkart.git" ] || [ -f "${APP_ROOT}/data/supertuxkart.1.5" ]; then
        if [ -d "${APP_ROOT}/data/tracks" ] && [ -d "${APP_ROOT}/data/karts" ]; then
            echo "${APP_ROOT}"
            return 0
        fi
    fi
    # 2) Flathub SuperTuxKart share tree
    for cand in \
        /var/lib/flatpak/app/net.supertuxkart.SuperTuxKart/current/active/files/share/supertuxkart \
        /var/lib/flatpak/app/net.supertuxkart.SuperTuxKart/*/stable/*/files/share/supertuxkart
    do
        # shellcheck disable=SC2086
        for p in $cand; do
            if [ -d "$p/data/tracks" ] && [ -d "$p/data/karts" ]; then
                echo "$p"
                return 0
            fi
        done
    done
    # 3) Previously prepared user runtime
    if [ -d "${RUNTIME_ROOT}/data/tracks" ]; then
        echo "${RUNTIME_ROOT}"
        return 0
    fi
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
        base="$(basename "$item")"
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

    # Version stamp for git builds
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

    # Overlay glass icons from the package
    if [ -d "${APP_ROOT}/data/gui/icons/android" ]; then
        for d in "${DEST}/data/gui/icons/android" \
                 "${DEST}/data/skins"/*/data/gui/icons/android; do
            [ -d "$d" ] || continue
            cp -f "${APP_ROOT}/data/gui/icons/android"/glass_*.png "$d/" 2>/dev/null || true
        done
    fi
}

STK_PREFIX="$(discover_stk_data || true)"
if [ -z "${STK_PREFIX:-}" ]; then
    echo "supertuxkart-touch: game assets not found." >&2
    echo "Install Flathub SuperTuxKart once, or place a full data tree under ${APP_ROOT}/data" >&2
    exit 1
fi

# If discovering Flathub (not already our runtime), rebuild overlay
case "$STK_PREFIX" in
    "$RUNTIME_ROOT") ;;
    *)
        prepare_runtime "$STK_PREFIX"
        STK_PREFIX="$RUNTIME_ROOT"
        ;;
esac

# Screen size hint for tablets (override with STK_TOUCH_SCREENSIZE)
SCREENSIZE="${STK_TOUCH_SCREENSIZE:-}"
if [ -z "$SCREENSIZE" ] && command -v xrandr >/dev/null 2>&1; then
    SCREENSIZE="$(xrandr 2>/dev/null | awk '/\*/{print $1; exit}')"
fi
EXTRA_ARGS=""
if [ -n "$SCREENSIZE" ]; then
    EXTRA_ARGS="--screensize=${SCREENSIZE}"
fi

export STK_TOUCH_PERF="$PERF"
export SUPERTUXKART_DATADIR="$STK_PREFIX"

# Prefer config under xdg for the touch app id when sandboxed
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"

cd "$(dirname "$BIN")"
# shellcheck disable=SC2086
exec "$BIN" $EXTRA_ARGS "$@"
