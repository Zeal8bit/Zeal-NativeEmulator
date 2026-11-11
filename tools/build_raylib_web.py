#!/usr/bin/env python3
"""Rebuild the bundled web dependency from the pinned, unmodified release."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

VERSION = '6.2'
EMSCRIPTEN = '7.0.0'
SHA256 = 'df7a13129b1bec9e73a6962efff170beba310ee6eab8f94cd000b7316b4db5ee'
URL = f'https://github.com/raysan5/raylib/archive/refs/tags/{VERSION}.tar.gz'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, help='Use a previously downloaded source archive')
    args = parser.parse_args()
    version = subprocess.check_output(['emcc', '--version'], text=True).splitlines()[0]
    if f' {EMSCRIPTEN} ' not in version:
        raise SystemExit(f'Activate Emscripten {EMSCRIPTEN} first; found: {version}')
    with tempfile.TemporaryDirectory(prefix='zeal-raylib-') as temporary:
        work = Path(temporary)
        archive = args.archive or work / 'raylib.tar.gz'
        if not args.archive:
            urllib.request.urlretrieve(URL, archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
            raise SystemExit('Raylib source checksum mismatch')
        with tarfile.open(archive) as source:
            source.extractall(work, filter='data')
        src = work / f'raylib-{VERSION}' / 'src'
        subprocess.run(['make', '-C', str(src), '-j8', 'PLATFORM=PLATFORM_WEB',
                        'GRAPHICS=GRAPHICS_API_OPENGL_ES3', 'RAYLIB_BUILD_MODE=RELEASE'], check=True)
        output = Path(__file__).resolve().parent.parent / 'raylib' / 'web'
        for header in ('raylib.h', 'raymath.h', 'rlgl.h'):
            shutil.copyfile(src / header, output / 'include' / header)
        shutil.copyfile(src / 'libraylib.web.a', output / 'lib' / 'libraylib.a')
        print(f'Updated {output} with Raylib {VERSION}, Emscripten {EMSCRIPTEN}')


if __name__ == '__main__':
    main()
