# Convenience targets for the default Meson build.
.PHONY: all clean

all: build/build.ninja
	meson compile -C build

build/build.ninja:
	meson setup build

clean:
	if test -f build/build.ninja; then meson compile -C build --clean; fi
