bin = zeal.elf
src = $(wildcard hw/*.c)
obj = $(patsubst hw/%.c, build/%.o, $(src))

CFLAGS = $(CFLAGS_EXTRA) -g -Wall -Wextra -O2 -std=c99 -pedantic -Iinclude/ -D_POSIX_C_SOURCE=200809L
CC = gcc

.PHONY: all clean

all: $(bin)

# Rule for building the final binary
$(bin): $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

# Rule for compiling object files into the build directory
build/%.o: hw/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -rf build
