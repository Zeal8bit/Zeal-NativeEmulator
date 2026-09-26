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
env = dict(os.environ, ZEAL_CONFIG_DIR=str(config), ZEAL_FLTK_SMOKE=str(build / "fltk-smoke"))
result = subprocess.run([str(build / "zeal-native"), "--debug", "--rom", str(root / "roms/default.img"), "--config", str(config / "zeal.ini")], cwd=root, env=env, timeout=20, capture_output=True, text=True)
print(result.stdout)
print(result.stderr, file=sys.stderr)
if result.returncode or "FLTK_SMOKE_OK" not in result.stdout:
    raise SystemExit(result.returncode or 1)
