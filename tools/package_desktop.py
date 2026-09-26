#!/usr/bin/env python3
"""Stage desktop binaries, assets, and non-system GUI dependencies; no install step."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import urllib.request

root = Path(__file__).resolve().parents[1]
build = (root / sys.argv[1]).resolve()
platform = sys.argv[2]
if not build.is_relative_to(root):
    raise SystemExit("Build directory must be inside project")
package = build / "package"
package.mkdir(exist_ok=True)
name = "zeal-native.exe" if platform == "windows" else "zeal-native"
executable = package / name
shutil.copy2(build / name, executable)
shutil.copytree(build / "assets", package / "assets", dirs_exist_ok=True)
shutil.copy2(root / "LICENSE", package / "LICENSE")
shutil.copy2(root / "ui/fltk/README.md", package / "FLTK-README.md")
shutil.copy2(root / "raylib/LICENSE.txt", package / "RAYLIB-LICENSE.txt")
fltk_license = root / "external/fltk-1.4.5/COPYING"
if fltk_license.exists():
    shutil.copy2(fltk_license, package / "FLTK-LICENSE.txt")
else:
    with urllib.request.urlopen("https://raw.githubusercontent.com/fltk/fltk/release-1.4.5/COPYING", timeout=30) as response:
        (package / "FLTK-LICENSE.txt").write_bytes(response.read())
if platform == "macos":
    libdir = package / "lib"
    libdir.mkdir(exist_ok=True)
    processed = set()
    def bundle(binary):
        if binary in processed:
            return
        processed.add(binary)
        output = subprocess.check_output(["otool", "-L", str(binary)], text=True)
        for line in output.splitlines()[1:]:
            dependency = line.strip().split(" (", 1)[0]
            if dependency.startswith(("/System/", "/usr/lib/", "@executable_path/")):
                continue
            source = Path(dependency)
            if not source.is_absolute():
                raise RuntimeError(f"Unresolved dependency {dependency}: bundle its rpath explicitly")
            destination = libdir / source.name
            if destination != binary:
                if not destination.exists():
                    shutil.copy2(source, destination)
                    bundle(destination)
                subprocess.run(["install_name_tool", "-change", dependency,
                                "@executable_path/lib/" + source.name, str(binary)], check=True)
        if binary != executable:
            subprocess.run(["install_name_tool", "-id", "@executable_path/lib/" + binary.name,
                            str(binary)], check=True)
        subprocess.run(["codesign", "--force", "--sign", "-", str(binary)], check=True)
    bundle(executable)
elif platform == "windows":
    for dll in (root / "raylib/win64").rglob("*.dll"):
        shutil.copy2(dll, package / dll.name)
elif platform == "linux":
    libdir = package / "lib"
    libdir.mkdir(exist_ok=True)
    for library in (root / "raylib/linux64/lib").glob("*.so*"):
        shutil.copy2(library, libdir / library.name)
    launcher = package / "run-zeal"
    launcher.write_text('#!/bin/sh\napp_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                        'export LD_LIBRARY_PATH="$app_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
                        'exec "$app_dir/zeal-native" "$@"\n')
    launcher.chmod(0o755)
else:
    raise SystemExit("Expected macos, windows, or linux")
print(package)
