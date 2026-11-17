#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
#
# SPDX-License-Identifier: Apache-2.0

"""Build a static SDL3 from the pinned, unmodified release for zeal-fltk.

For systems without an SDL3 package (for example Ubuntu 22.04 and 24.04).
The result installs into sdl3/<platform>/ (ignored by Git); configure with

    meson setup build-fltk -Dfrontend=sdl-fltk \\
        -Dpkg_config_path=$PWD/sdl3/<platform>/lib/pkgconfig -Dsdl3_static=true

Audio and input backends (PulseAudio, PipeWire, ALSA, udev, ...) are loaded
at runtime by SDL, so the binary does not require them to be installed.
"""

import argparse
import hashlib
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

VERSION = "3.4.20"
SHA256 = "f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f"
URL = f"https://github.com/libsdl-org/SDL/releases/download/release-{VERSION}/SDL3-{VERSION}.tar.gz"


def default_prefix():
    system = platform.system().lower()
    machine = platform.machine().lower()
    arch = {"x86_64": "64", "amd64": "64", "aarch64": "arm64", "arm64": "arm64"}.get(machine, machine)
    return Path(__file__).resolve().parent.parent / "sdl3" / f"{system}{arch}"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--archive", type=Path, help="Use a previously downloaded source archive")
    parser.add_argument("--prefix", type=Path, default=default_prefix(), help="Install prefix (default: %(default)s)")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    arguments = parser.parse_args()

    if shutil.which("cmake") is None:
        raise SystemExit("cmake is required to build SDL3")

    with tempfile.TemporaryDirectory(prefix="zeal-sdl3-") as temporary_directory:
        work = Path(temporary_directory)
        archive = arguments.archive or work / f"SDL3-{VERSION}.tar.gz"
        if not arguments.archive:
            print(f"Downloading {URL}")
            urllib.request.urlretrieve(URL, archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
            raise SystemExit("SDL3 source checksum mismatch")
        with tarfile.open(archive) as source_archive:
            if hasattr(tarfile, "data_filter"):
                source_archive.extractall(work, filter="data")
            else:  # Python without the extraction filter backport
                source_archive.extractall(work)
        source = work / f"SDL3-{VERSION}"
        build = work / "build"
        prefix = arguments.prefix.resolve()
        subprocess.run(
            [
                "cmake", "-S", str(source), "-B", str(build),
                "-DCMAKE_BUILD_TYPE=Release",
                f"-DCMAKE_INSTALL_PREFIX={prefix}",
                "-DCMAKE_INSTALL_LIBDIR=lib",
                "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
                "-DSDL_SHARED=OFF",
                "-DSDL_STATIC=ON",
                "-DSDL_TEST_LIBRARY=OFF",
                "-DSDL_TESTS=OFF",
                "-DSDL_EXAMPLES=OFF",
                # zeal-fltk uses SDL only for audio and gamepads (FLTK owns the
                # windows), so do not require X11/Wayland development files.
                "-DSDL_UNIX_CONSOLE_BUILD=ON",
            ],
            check=True,
        )
        subprocess.run(["cmake", "--build", str(build), "--parallel", str(arguments.jobs)], check=True)
        if prefix.exists():
            shutil.rmtree(prefix)
        subprocess.run(["cmake", "--install", str(build)], check=True)
        license_file = source / "LICENSE.txt"
        if license_file.exists():
            shutil.copy(license_file, prefix / "LICENSE.txt")
    print(f"SDL3 {VERSION} installed in {prefix}")
    print(f"Configure with: -Dpkg_config_path={prefix / 'lib' / 'pkgconfig'} -Dsdl3_static=true")


if __name__ == "__main__":
    main()
