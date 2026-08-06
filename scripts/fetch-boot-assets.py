#!/usr/bin/env python3
"""Fetch the minimal SuperTuxKart asset directories needed to reach the main menu.

The packaged app is deliberately slim: tracks and karts are pulled at first launch
by the in-engine DownloadAssets wizard. That wizard lives in the GUI, so the engine
has to finish booting before the user ever sees it -- and a few asset directories
are load-bearing during boot. Most importantly SFXManager treats a missing
``sfx/sfx.xml`` as *fatal*, so a package shipping empty asset stubs dies on launch
and the wizard never appears.

This script downloads only the required directories out of the official
``stk-assets-mobile`` release zip using HTTP range requests, so a build pulls a few
megabytes instead of the full ~228 MB archive.
"""

from __future__ import annotations

import argparse
import io
import re
import sys
import urllib.request
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

DEFAULT_URL_BASE = "https://github.com/supertuxkart/stk-assets-mobile/releases/download"

# Directories the engine needs before the download wizard can be shown. Missing
# `sfx/sfx.xml` and `textures/materials.xml` are both hard failures, and the menus
# expect at least one kart to exist. `tracks` (141 MB) and `music` (48 MB) are the
# bulk of the archive and are absent-tolerant, so those stay with the wizard.
DEFAULT_BOOT_DIRS = ("sfx", "models", "textures", "library", "karts")

# A directory is considered already staged when it holds at least this many files.
POPULATED_THRESHOLD = 1


class HttpRangeFile(io.RawIOBase):
    """A seekable read-only file backed by HTTP range requests.

    Lets :mod:`zipfile` read a remote archive's central directory and then extract
    individual members without downloading the whole archive.
    """

    def __init__(self, url: str) -> None:
        request = urllib.request.Request(url, method="HEAD")
        with urllib.request.urlopen(request) as response:
            self.size = int(response.headers["Content-Length"])
            # Follow the redirect once so every range request skips it.
            self.url = response.url
        self.position = 0

    def seekable(self) -> bool:
        return True

    def readable(self) -> bool:
        return True

    def tell(self) -> int:
        return self.position

    def seek(self, offset: int, whence: int = io.SEEK_SET) -> int:
        if whence == io.SEEK_SET:
            self.position = offset
        elif whence == io.SEEK_CUR:
            self.position += offset
        else:
            self.position = self.size + offset
        return self.position

    def readinto(self, buffer) -> int:
        wanted = len(buffer)
        if wanted == 0 or self.position >= self.size:
            return 0
        last_byte = min(self.position + wanted, self.size) - 1
        request = urllib.request.Request(
            self.url, headers={"Range": f"bytes={self.position}-{last_byte}"}
        )
        with urllib.request.urlopen(request) as response:
            chunk = response.read()
        buffer[: len(chunk)] = chunk
        self.position += len(chunk)
        return len(chunk)


def detect_asset_version() -> str:
    """Read PROJECT_VERSION from the engine CMakeLists, which becomes STK_VERSION.

    The engine builds its download URL as ``<base>/<STK_VERSION>/stk-assets.zip``, so
    the build must stage assets from the very same release the wizard will use.
    """
    cmake_lists = REPO_ROOT / "engine" / "CMakeLists.txt"
    match = re.search(
        r'set\(PROJECT_VERSION\s+"([^"]+)"\)', cmake_lists.read_text(encoding="utf-8")
    )
    if not match:
        raise SystemExit(f"Could not read PROJECT_VERSION from {cmake_lists}")
    return match.group(1)


def is_populated(directory: Path) -> bool:
    """True when a destination directory already holds staged assets."""
    if not directory.is_dir():
        return False
    return sum(1 for _ in directory.rglob("*") if _.is_file()) >= POPULATED_THRESHOLD


def open_archive(source: str) -> zipfile.ZipFile:
    """Open the asset archive from a local path or over HTTP without full download."""
    if "://" not in source:
        return zipfile.ZipFile(source)
    return zipfile.ZipFile(io.BufferedReader(HttpRangeFile(source), buffer_size=512 * 1024))


def extract_directories(
    archive: zipfile.ZipFile, wanted: tuple[str, ...], destination: Path
) -> int:
    """Extract the wanted top-level directories into ``destination``.

    Returns the number of bytes written, for build log visibility.
    """
    prefixes = tuple(f"{name}/" for name in wanted)
    members = [m for m in archive.infolist() if m.filename.startswith(prefixes)]
    if not members:
        raise SystemExit(
            f"Archive contains none of {wanted}; top-level entries: "
            f"{sorted({n.split('/')[0] for n in archive.namelist()})}"
        )

    written = 0
    for member in members:
        target = destination / member.filename
        if member.is_dir():
            target.mkdir(parents=True, exist_ok=True)
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        with archive.open(member) as source_file, open(target, "wb") as output_file:
            while chunk := source_file.read(256 * 1024):
                output_file.write(chunk)
                written += len(chunk)
    return written


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dest",
        required=True,
        help="Data directory to stage into (the one that becomes <prefix>/data).",
    )
    parser.add_argument(
        "--version",
        default=None,
        help="Asset release tag; defaults to the engine's PROJECT_VERSION.",
    )
    parser.add_argument(
        "--dirs",
        default=",".join(DEFAULT_BOOT_DIRS),
        help="Comma-separated top-level asset directories to stage.",
    )
    parser.add_argument(
        "--source",
        default=None,
        help="Override archive location (local zip path or URL).",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Re-download even when the destination already looks populated.",
    )
    args = parser.parse_args()

    destination = Path(args.dest)
    wanted = tuple(name for name in args.dirs.split(",") if name)

    missing = [name for name in wanted if not is_populated(destination / name)]
    if not missing and not args.force:
        print(f"fetch-boot-assets: {', '.join(wanted)} already staged in {destination}")
        return 0

    version = args.version or detect_asset_version()
    source = args.source or f"{DEFAULT_URL_BASE}/{version}/stk-assets.zip"

    print(f"fetch-boot-assets: staging {', '.join(missing)} from {source}")
    archive = open_archive(source)
    written = extract_directories(archive, tuple(missing), destination)
    print(f"fetch-boot-assets: wrote {written / 1e6:.1f} MB to {destination}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
