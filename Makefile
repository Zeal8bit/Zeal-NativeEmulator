bin = zeal.elf
src = $(wildcard hw/*.c hw/zvb/*.c)
obj = $(patsubst hw/%.c, build/%.o, $(src))

ifeq ($(RAYLIB_PATH),)
RAYLIB_PATH := $(shell pwd)/raylib
endif

CFLAGS = $(CFLAGS_EXTRA) -g -Wall -Wextra -O2 -std=c99 -Iinclude/ -I$(RAYLIB_PATH)/include -D_POSIX_C_SOURCE=200809L
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

clean:
	-rm -rf build
	-rm -rf $(bin)
