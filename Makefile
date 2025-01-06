# RAYLIB_PATH = $(shell pwd)/raylib
RAYGUI_PATH = raygui/src

bin = zeal.elf
src = $(wildcard hw/*.c hw/zvb/*.c utils/*.c)
obj = $(patsubst hw/%.c, build/%.o, $(patsubst utils/%.c, build/utils/%.o, $(src)))
obj += build/raygui.o

ifeq ($(RAYLIB_PATH),)
RAYLIB_PATH := $(shell pwd)/raylib
endif

CFLAGS = $(CFLAGS_EXTRA) -g -Wall -Wextra -O2 -std=c99 -Iinclude/ -I$(RAYLIB_PATH)/include -I$(RAYGUI_PATH) -D_POSIX_C_SOURCE=200809L
LDFLAGS = -L$(RAYLIB_PATH)/lib -lraylib
CC = gcc

.PHONY: all clean

all: $(bin)

# Rule for building the final binary
$(bin): $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

# Rule for compiling object files into the build directory
build/%.o: hw/%.c
	@mkdir -p build $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@


build/utils/%.o: utils/%.c
	@mkdir -p build $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@


build/raygui.o: $(RAYGUI_PATH)/raygui.c  # Rule for compiling raygui.c explicitly
	@mkdir -p build $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -rf build
	-rm -rf $(bin)
