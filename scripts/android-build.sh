#!/bin/bash
# Build the SuperTuxKart Touch Android APK (arm64-v8a / armeabi-v7a).
#
# Wraps the engine's own Android build (engine/android/{make_deps,generate_assets,make}.sh)
# and re-brands it: our package id, our app name, and the same logo the Click and
# Flatpak packages use (engine/data/supertuxkart_512.png).
#
# The engine build wants three things it does not carry in-tree:
#   * android-sdk / android-ndk symlinks next to engine/android
#   * lib/<dep> sources from the upstream dependencies release
#   * an assets tree (karts + tracks) to fold into assets/
# Each of those is fetched here and cached under build/android.
set -euo pipefail

ROOT="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
ANDROID_DIR="$ROOT/engine/android"
CACHE_DIR="${ANDROID_CACHE_DIR:-$ROOT/build/android}"
OUT_DIR="${OUT_DIR:-$ROOT/build/android-out}"

# Branding — consumed by engine/android/make.sh through the ${VAR:-default} hooks.
export APP_NAME_RELEASE="${ANDROID_APP_NAME:-SuperTuxKart Touch}"
export PACKAGE_NAME_RELEASE="${ANDROID_PACKAGE_NAME:-io.github.dixonsolutions.supertuxkarttouch}"
export PACKAGE_CLASS_NAME_RELEASE="$(printf '%s' "$PACKAGE_NAME_RELEASE" | tr '.' '/')"
export APP_DIR_NAME_RELEASE="supertuxkart-touch"

# Upstream artefact versions. STK's CMake PROJECT_VERSION picks the asset release,
# because the in-game download wizard builds its URL from the same string.
STK_VERSION="$(sed -n 's/^set(PROJECT_VERSION "\(.*\)")$/\1/p' "$ROOT/engine/CMakeLists.txt" | head -1)"
STK_VERSION="${STK_VERSION:-git}"
DEPS_RELEASE="${STK_DEPS_RELEASE:-preview}"
ASSETS_RELEASE="${STK_ASSETS_RELEASE:-$STK_VERSION}"
ASSETS_ZIP="${STK_ASSETS_ZIP:-stk-assets.zip}"
export STK_NDK_VERSION="${STK_NDK_VERSION:-28.1.13356709}"
export STK_MIN_ANDROID_SDK="${STK_MIN_ANDROID_SDK:-21}"
export STK_TARGET_ANDROID_SDK="${STK_TARGET_ANDROID_SDK:-35}"

ARCHS="${ANDROID_ARCHS:-aarch64 armv7}"
export PROJECT_VERSION="${PROJECT_VERSION:-$STK_VERSION}"
export PROJECT_CODE="${PROJECT_CODE:-1}"

log() { printf '\n== %s\n' "$*"; }

usage() {
    cat <<EOF
Usage: $(basename "$0") [--arch "aarch64 armv7"] [--version X.Y.Z] [--code N] [--out DIR]

Environment:
  ANDROID_SDK_ROOT / ANDROID_HOME   Android SDK (required)
  ANDROID_NDK_ROOT                  NDK \$STK_NDK_VERSION (optional; else \$SDK/ndk/\$ver)
  STK_KEYSTORE/STK_STOREPASS/STK_ALIAS  Release signing key; a throwaway key is
                                    generated when unset (see docs/ANDROID.md).
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --arch) ARCHS="$2"; shift 2 ;;
        --version) PROJECT_VERSION="$2"; shift 2 ;;
        --code) PROJECT_CODE="$2"; shift 2 ;;
        --out) OUT_DIR="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
    esac
done

mkdir -p "$CACHE_DIR" "$OUT_DIR"

##### SDK / NDK ###############################################################

SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
if [ -z "$SDK_ROOT" ] || [ ! -d "$SDK_ROOT" ]; then
    echo "Set ANDROID_SDK_ROOT (or ANDROID_HOME) to an Android SDK install." >&2
    exit 1
fi
SDK_ROOT="$(cd "$SDK_ROOT" && pwd)"

# The engine is pinned to one NDK revision. CI runners preset ANDROID_NDK_ROOT
# to whatever they shipped with, so an SDK-managed copy of the pinned version
# wins over the environment; ANDROID_NDK_ROOT is only trusted if it matches.
ndk_revision() {
    [ -f "$1/source.properties" ] || return 1
    sed -n 's/^Pkg.Revision *= *//p' "$1/source.properties" | tr -d '\r'
}

NDK_ROOT=""
if [ -d "$SDK_ROOT/ndk/$STK_NDK_VERSION" ]; then
    NDK_ROOT="$SDK_ROOT/ndk/$STK_NDK_VERSION"
elif [ -n "${ANDROID_NDK_ROOT:-}" ] && [ "$(ndk_revision "$ANDROID_NDK_ROOT" || true)" = "$STK_NDK_VERSION" ]; then
    NDK_ROOT="$ANDROID_NDK_ROOT"
fi
if [ -z "$NDK_ROOT" ]; then
    echo "Missing NDK $STK_NDK_VERSION — install it with:" >&2
    echo "  sdkmanager \"ndk;$STK_NDK_VERSION\"" >&2
    echo "Found instead: $(ls -1 "$SDK_ROOT/ndk" 2>/dev/null | tr '\n' ' ')${ANDROID_NDK_ROOT:+ (ANDROID_NDK_ROOT=$ANDROID_NDK_ROOT)}" >&2
    exit 1
fi
NDK_ROOT="$(cd "$NDK_ROOT" && pwd)"

# make.sh/make_deps.sh append the version to NDK_PATH, so point at a parent dir
# holding a "<version>" entry rather than at the NDK itself.
NDK_PARENT="$CACHE_DIR/ndk"
mkdir -p "$NDK_PARENT"
ln -sfn "$NDK_ROOT" "$NDK_PARENT/$STK_NDK_VERSION"
export NDK_PATH="$NDK_PARENT"
export SDK_PATH="$SDK_ROOT"
export ANDROID_HOME="$SDK_ROOT"

##### Signing key #############################################################

if [ -z "${STK_KEYSTORE:-}" ]; then
    STK_KEYSTORE="$CACHE_DIR/throwaway.keystore"
    STK_STOREPASS="${STK_STOREPASS:-supertuxtouch}"
    STK_ALIAS="${STK_ALIAS:-supertuxtouch}"
    if [ ! -f "$STK_KEYSTORE" ]; then
        log "No STK_KEYSTORE set — generating a throwaway signing key"
        echo "   Installs from different builds will NOT upgrade in place." >&2
        keytool -genkeypair -v \
            -keystore "$STK_KEYSTORE" \
            -storepass "$STK_STOREPASS" -keypass "$STK_STOREPASS" \
            -alias "$STK_ALIAS" \
            -keyalg RSA -keysize 2048 -validity 10000 \
            -dname "CN=SuperTuxKart Touch, OU=CI, O=dixonSolutions, C=US" >/dev/null
    fi
fi
export STK_KEYSTORE STK_STOREPASS STK_ALIAS
export BUILD_TYPE=release

##### Upstream dependency sources #############################################

DEPS_TARBALL="$CACHE_DIR/dependencies-android-src-$DEPS_RELEASE.tar.xz"
DEPS_STAMP="$ROOT/engine/lib/.android-deps-$DEPS_RELEASE"
if [ ! -f "$DEPS_STAMP" ]; then
    if [ ! -f "$DEPS_TARBALL" ]; then
        log "Fetching Android dependency sources ($DEPS_RELEASE)"
        curl -fL --retry 3 --retry-delay 5 -o "$DEPS_TARBALL.part" \
            "https://github.com/supertuxkart/dependencies/releases/download/$DEPS_RELEASE/dependencies-android-src.tar.xz"
        mv "$DEPS_TARBALL.part" "$DEPS_TARBALL"
    fi
    log "Unpacking dependency sources into engine/lib"
    tar -xf "$DEPS_TARBALL" -C "$ROOT/engine/lib" --strip-components=1
    touch "$DEPS_STAMP"
fi

if [ ! -d "$ROOT/engine/lib/sdl2/android-project" ]; then
    echo "engine/lib/sdl2/android-project missing — the dependency tarball did not unpack as expected." >&2
    ls "$ROOT/engine/lib" >&2
    exit 1
fi

##### Game assets #############################################################

ASSETS_SRC="$CACHE_DIR/stk-assets"
if [ ! -d "$ASSETS_SRC/tracks" ]; then
    ASSETS_ARCHIVE="$CACHE_DIR/$ASSETS_RELEASE-$ASSETS_ZIP"
    if [ ! -f "$ASSETS_ARCHIVE" ]; then
        log "Fetching $ASSETS_ZIP ($ASSETS_RELEASE)"
        curl -fL --retry 3 --retry-delay 5 -o "$ASSETS_ARCHIVE.part" \
            "https://github.com/supertuxkart/stk-assets-mobile/releases/download/$ASSETS_RELEASE/$ASSETS_ZIP"
        mv "$ASSETS_ARCHIVE.part" "$ASSETS_ARCHIVE"
    fi
    log "Unpacking game assets"
    rm -rf "$ASSETS_SRC.tmp"
    mkdir -p "$ASSETS_SRC.tmp"
    unzip -q "$ASSETS_ARCHIVE" -d "$ASSETS_SRC.tmp"
    # The archive is either flat (karts/ tracks/ ...) or nested one level.
    if [ ! -d "$ASSETS_SRC.tmp/tracks" ]; then
        inner="$(find "$ASSETS_SRC.tmp" -maxdepth 2 -type d -name tracks -print -quit)"
        test -n "$inner" || { echo "No tracks/ directory in $ASSETS_ARCHIVE" >&2; exit 1; }
        mv "$(dirname "$inner")" "$ASSETS_SRC.staged"
        rm -rf "$ASSETS_SRC.tmp"
        mv "$ASSETS_SRC.staged" "$ASSETS_SRC"
    else
        rm -rf "$ASSETS_SRC"
        mv "$ASSETS_SRC.tmp" "$ASSETS_SRC"
    fi
fi

if [ ! -f "$ANDROID_DIR/assets/has_assets.txt" ]; then
    log "Generating APK assets tree"
    # The mobile asset release is already downscaled, so skip the (very slow)
    # re-encode passes and just fold in engine/data.
    ( cd "$ANDROID_DIR" && \
      ASSETS_PATHS="$ASSETS_SRC" \
      DECREASE_QUALITY=0 \
      CONVERT_TO_JPG=0 \
      RUN_OPTIMIZE_SCRIPT=0 \
      ./generate_assets.sh )
fi

# make.sh refuses to build unless the assets tree carries a marker file matching
# PROJECT_VERSION; engine/data ships supertuxkart.git for the git series.
if [ ! -f "$ANDROID_DIR/assets/data/supertuxkart.$PROJECT_VERSION" ]; then
    printf '%s\n' "$APP_NAME_RELEASE" > "$ANDROID_DIR/assets/data/supertuxkart.$PROJECT_VERSION"
fi

##### Icons ###################################################################

ICON_SRC="${ANDROID_ICON_SRC:-$ROOT/engine/data/supertuxkart_512.png}"
test -f "$ICON_SRC" || { echo "Missing logo: $ICON_SRC" >&2; exit 1; }
MAGICK="$(command -v magick || command -v convert)"
test -n "$MAGICK" || { echo "ImageMagick (magick/convert) is required" >&2; exit 1; }

BRAND_DIR="$CACHE_DIR/branding"
mkdir -p "$BRAND_DIR"
# Launcher icon: the same logo the Click/Flatpak packages ship.
"$MAGICK" "$ICON_SRC" -resize 512x512 "$BRAND_DIR/icon.png"
# Adaptive foreground: Android crops adaptive icons to the inner ~66%, so inset
# the logo on a transparent 512px canvas or the wheels get shaved off.
"$MAGICK" "$ICON_SRC" -resize 340x340 \
    -background none -gravity center -extent 512x512 "$BRAND_DIR/icon_adaptive_fg.png"
export APP_ICON_RELEASE="$BRAND_DIR/icon.png"
export APP_ICON_ADAPTIVE_FG_RELEASE="$BRAND_DIR/icon_adaptive_fg.png"

##### Build ###################################################################

# Android.mk links these as prebuilts. make_deps.sh swallows its own failures
# (its check_error calls a bare `exit`, which exits 0), so a broken dependency
# build shows up as "LOCAL_SRC_FILES points to a missing file" pages later —
# check the outputs here instead.
REQUIRED_DEPS="openal/libopenal.a libogg/libogg.a libvorbis/lib/libvorbis.a \
libvorbis/lib/libvorbisfile.a curl/lib/libcurl.a mbedtls/library/libmbedtls.a \
mbedtls/library/libmbedcrypto.a mbedtls/library/libmbedx509.a libjpeg/libjpeg.a \
zlib/libz.a libpng/libpng.a freetype/build/libfreetype.a \
harfbuzz/build/libharfbuzz.a shaderc/libshaderc/libshaderc_combined.a \
libsquish/libsquish.a astc-encoder/Source/libastcenc.a"

for arch in $ARCHS; do
    case "$arch" in
        aarch64|arm64-v8a) abi=arm64-v8a ;;
        armv7|armeabi-v7a) abi=armeabi-v7a ;;
        *) abi="$arch" ;;
    esac

    log "Building native dependencies for $abi"
    # make_deps.sh reads COMPILE_ARCH, not $1 — passing the arch positionally
    # silently builds all four ABIs and then runs out of patience.
    ( cd "$ANDROID_DIR" && COMPILE_ARCH="$arch" ./make_deps.sh )

    missing=""
    for dep in $REQUIRED_DEPS; do
        [ -f "$ANDROID_DIR/deps-$abi/$dep" ] || missing="$missing $dep"
    done
    if [ -n "$missing" ]; then
        echo "make_deps.sh did not produce:$missing" >&2
        echo "See the log above for the first failing dependency." >&2
        exit 1
    fi

    log "Building APK for $abi"
    ( cd "$ANDROID_DIR" && COMPILE_ARCH="$arch" ./make.sh "-j$(nproc)" )

    # Every ABI lands on the same gradle output path, so take the freshest file.
    apk="$(find "$ANDROID_DIR/build/outputs/apk/release" -name '*.apk' -printf '%T@ %p\n' 2>/dev/null \
           | sort -rn | head -1 | cut -d' ' -f2-)"
    test -n "$apk" || { echo "No APK produced for $abi" >&2; find "$ANDROID_DIR/build/outputs" -type f >&2 || true; exit 1; }
    cp -v "$apk" "$OUT_DIR/SuperTuxKartTouch-$PROJECT_VERSION-$abi.apk"
done

log "APKs in $OUT_DIR"
ls -lh "$OUT_DIR"
