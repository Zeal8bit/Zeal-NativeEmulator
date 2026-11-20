#!/usr/bin/env python3

import shutil
import sys
from pathlib import Path


def main() -> int:
    destination = Path(sys.argv[1])
    stamp = Path(sys.argv[2])
    sources = [Path(source) for source in sys.argv[3:]]

    destination.mkdir(parents=True, exist_ok=True)
    for source in sources:
        shutil.copy2(source, destination / source.name)
    stamp.touch()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
