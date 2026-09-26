#!/usr/bin/env sh
# Build pinned FLTK in the project without installing it. Used by desktop CI.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_dir="$root/external/fltk-1.4.5"
mkdir -p "$root/external"
if [ ! -f "$source_dir/configure" ]; then
    curl --fail --location --retry 3 https://www.fltk.org/pub/fltk/1.4.5/fltk-1.4.5-source.tar.gz -o "$root/external/fltk.tar.gz"
    tar -xzf "$root/external/fltk.tar.gz" -C "$root/external"
fi
cd "$source_dir"
if [ "${1:-}" = windows ]; then
    ./configure --host=x86_64-w64-mingw32 --disable-shared --disable-gl --disable-wayland --enable-localjpeg --enable-localpng --enable-localzlib
else
    ./configure --disable-shared --disable-gl --disable-wayland --enable-localjpeg --enable-localpng --enable-localzlib
fi
make -j2 -C src
printf '%s\n' "FLTK built in $source_dir; use -Dfltk_config=$source_dir/fltk-config"
