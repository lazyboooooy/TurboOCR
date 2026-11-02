#!/usr/bin/env python3
"""TurboOCR v1.1.0 dependency downloader.

Downloads the prebuilt 3rdparty/ + models/ archive from GitHub Releases.
The archive is about 336 MB (extracted); no Paddle/CUDA runtime is included.

Usage:
    python setup_deps.py
    python setup_deps.py --tag v1.1.0
    python setup_deps.py --local .\deps-v1.1.0.zip
    python setup_deps.py --check
"""

import argparse
import hashlib
import shutil
import sys
import zipfile
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import urlopen

REPO = "lazyboooooy/TurboOCR"
DEFAULT_TAG = "v1.1.0"
ARCHIVE = "deps-v1.1.0.zip"
EXPECTED_SHA256 = "312a6d3e76d5414693410f85c94f20ae6eddda7c71d71d14bc5aad9a5cf907f6"
ROOT = Path(__file__).resolve().parent
REQUIRED_DIRS = ["3rdparty", "models"]
CHUNK = 1024 * 1024


def verify() -> bool:
    ok = True
    for name in REQUIRED_DIRS:
        path = ROOT / name
        if not path.exists() or not any(path.iterdir()):
            print(f"  x {name}/ missing or empty")
            ok = False
            continue
        size = sum(f.stat().st_size for f in path.rglob("*") if f.is_file())
        print(f"  ok {name}/ ({size / (1024 * 1024):.0f} MB)")
    return ok


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(CHUNK), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path) -> None:
    print(f"  download {url}")
    with urlopen(url) as resp:
        total = int(resp.headers.get("Content-Length", 0))
        done = 0
        with open(dest, "wb") as f:
            while True:
                chunk = resp.read(CHUNK)
                if not chunk:
                    break
                f.write(chunk)
                done += len(chunk)
                if total:
                    print(
                        f"\r  {done / 1048576:.0f}/{total / 1048576:.0f} MB "
                        f"({done / total * 100:.1f}%)",
                        end="",
                        flush=True,
                    )
    print()


def extract(archive: Path) -> None:
    print(f"  extract {archive.name} -> {ROOT}")
    with zipfile.ZipFile(archive) as zf:
        zf.extractall(ROOT)


def main() -> int:
    parser = argparse.ArgumentParser(description="TurboOCR dependency setup")
    parser.add_argument("--tag", default=DEFAULT_TAG)
    parser.add_argument("--local", help="local zip, or directory containing the zip")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    print("\nTurboOCR dependency setup\n")

    if args.check:
        return 0 if verify() else 1

    if not args.force and verify():
        print("\nDependencies already present; nothing to do.")
        return 0

    if args.local:
        local = Path(args.local)
        if local.is_dir():
            archive = local / ARCHIVE
        else:
            archive = local
        if not archive.is_file():
            print(f"  x archive not found: {archive}")
            return 1
    else:
        tmp = ROOT / ".deps_tmp"
        tmp.mkdir(exist_ok=True)
        archive = tmp / ARCHIVE
        url = f"https://github.com/{REPO}/releases/download/{args.tag}/{ARCHIVE}"
        try:
            download(url, archive)
        except (HTTPError, URLError, OSError) as e:
            print(f"  x download failed: {e}")
            print("  Download it manually and run:")
            print(f"    python setup_deps.py --local <path-to-{ARCHIVE}>")
            return 1

    actual = sha256(archive)
    print(f"  sha256 {actual}")
    if EXPECTED_SHA256 and actual.lower() != EXPECTED_SHA256.lower():
        print("  x sha256 mismatch")
        return 1

    extract(archive)
    if ".deps_tmp" in str(archive):
        shutil.rmtree(ROOT / ".deps_tmp", ignore_errors=True)

    print("\nVerification:")
    if not verify():
        print("\n  x dependency verification failed")
        return 1

    print("\nDone. Build:")
    print("  cmake -S . -B build -G \"Visual Studio 16 2019\" -A x64 -T v142")
    print("  cmake --build build --config Release --parallel")
    return 0


if __name__ == "__main__":
    sys.exit(main())
