#!/usr/bin/env python3
"""Bounded FLTK integration test. All generated files stay inside the build tree."""
from pathlib import Path
import os
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
build = (root / (sys.argv[1] if len(sys.argv) > 1 else "build-fltk")).resolve()
if not build.is_relative_to(root):
    raise SystemExit("Build directory must be inside project")
config = build / "smoke-config"
config.mkdir(exist_ok=True)
# Seed a dock layout that differs from the built-in default: Breakpoints (2) and VRAM (7)
# are hidden, so the View check marks must not claim they are open. Without this the test
# would compare the menu against the same default layout it was built from.
(config / "fltk-workspace.ini").write_text(
    "version=1\n"
    "hidden=132\n"
    "root=2,0.64,0\n"
    "roota=1,0.48,0\n"
    "rootaa=0,0.5,0,0\n"
    "rootab=1,0.46,0\n"
    "rootaba=0,0.5,0,1\n"
    "rootabb=0,0.5,0,3\n"
    "rootb=1,0.55,0\n"
    "rootba=0,0.5,0,4\n"
    "rootbb=1,0.46,0\n"
    "rootbba=0,0.5,0,5\n"
    "rootbbb=0,0.5,0,6\n"
)
env = dict(os.environ, ZEAL_CONFIG_DIR=str(config), ZEAL_FLTK_SMOKE=str(build / "fltk-smoke"))
result = subprocess.run([str(build / "zeal-native"), "--debug", "--rom", str(root / "roms/default.img"), "--config", str(config / "zeal.ini")], cwd=root, env=env, timeout=20, capture_output=True, text=True)
print(result.stdout)
print(result.stderr, file=sys.stderr)
if result.returncode or "FLTK_SMOKE_OK" not in result.stdout:
    raise SystemExit(result.returncode or 1)
