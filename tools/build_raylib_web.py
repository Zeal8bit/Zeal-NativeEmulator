#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 David Higgins <zoul0813@me.com>
#
# SPDX-License-Identifier: Apache-2.0

"""Build the web dependency from the pinned, unmodified Raylib release."""

import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

VERSION = "6.2"
EMSCRIPTEN = "7.0.0"
SHA256 = "df7a13129b1bec9e73a6962efff170beba310ee6eab8f94cd000b7316b4db5ee"
URL = f"https://github.com/raysan5/raylib/archive/refs/tags/{VERSION}.tar.gz"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--archive", type=Path, help="Use a previously downloaded source archive"
    )
    arguments = parser.parse_args()
    compiler_version = subprocess.check_output(["emcc", "--version"], text=True).splitlines()[0]
    if f" {EMSCRIPTEN} " not in compiler_version:
        raise SystemExit(f"Activate Emscripten {EMSCRIPTEN} first; found: {compiler_version}")
    with tempfile.TemporaryDirectory(prefix="zeal-raylib-") as temporary_directory:
        build_directory = Path(temporary_directory)
        archive = arguments.archive or build_directory / "raylib.tar.gz"
        if not arguments.archive:
            urllib.request.urlretrieve(URL, archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
            raise SystemExit("Raylib source checksum mismatch")
        with tarfile.open(archive) as source_archive:
            source_archive.extractall(build_directory, filter="data")
        source_directory = build_directory / f"raylib-{VERSION}" / "src"
        subprocess.run(
            [
                "make",
                "-C",
                str(source_directory),
                "-j8",
                "PLATFORM=PLATFORM_WEB",
                "GRAPHICS=GRAPHICS_API_OPENGL_ES3",
                "RAYLIB_BUILD_MODE=RELEASE",
            ],
            check=True,
        )
        output_directory = Path(__file__).resolve().parent.parent / "raylib" / "web"
        (output_directory / "include").mkdir(parents=True, exist_ok=True)
        (output_directory / "lib").mkdir(parents=True, exist_ok=True)
        for header in ("raylib.h", "raymath.h", "rlgl.h"):
            shutil.copyfile(source_directory / header, output_directory / "include" / header)
        shutil.copyfile(source_directory / "libraylib.web.a", output_directory / "lib" / "libraylib.a")
        print(f"Updated {output_directory} with Raylib {VERSION}, Emscripten {EMSCRIPTEN}")


if __name__ == "__main__":
    main()
