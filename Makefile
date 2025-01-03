bin = zeal.elf
dirs = hw hw/zvb dbg
src = $(wildcard hw/*.c hw/zvb/*.c dbg/*.c)
obj = $(patsubst hw/%.c, build/hw/%.o, $(patsubst dbg/%.c, build/dbg/%.o, $(src)))

RAYLIB_PATH = $(shell pwd)/raylib
CFLAGS = $(CFLAGS_EXTRA) -g -Wall -Wextra -std=c99 -O2 -pedantic -Iinclude/ -I$(RAYLIB_PATH)/include -D_POSIX_C_SOURCE=200809L -DCONFIG_ENABLE_GDB_SERVER
LDFLAGS = -L$(RAYLIB_PATH)/lib -lraylib
CC = gcc

.PHONY: all clean

all: $(bin)

# Rule for building the final binary
$(bin): $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

# Rule for compiling object files into the build directory
build/hw/%.o: hw/%.c
	@mkdir -p build $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/dbg/%.o: dbg/%.c
	@mkdir -p build $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -rf build
