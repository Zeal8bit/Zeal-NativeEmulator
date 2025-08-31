#!/usr/bin/env python3
import sys
from pathlib import Path

BYTES_PER_LINE = 40  # number of bytes per line

if len(sys.argv) != 2:
    print("Usage: file_to_string.py <input_file>", file=sys.stderr)
    sys.exit(1)

input_file = Path(sys.argv[1])

if not input_file.is_file():
    print(f"Error: file {input_file} does not exist", file=sys.stderr)
    sys.exit(1)

content = input_file.read_bytes()

# convert each byte to \xHH
hex_bytes = [f'\\x{b:02x}' for b in content]

# group into lines of BYTES_PER_LINE bytes
lines = []
for i in range(0, len(hex_bytes), BYTES_PER_LINE):
    chunk = hex_bytes[i:i + BYTES_PER_LINE]
    lines.append(''.join(chunk))

# join lines with newline and C string concatenation
output = '\n'.join(['"' + line + '"' for line in lines])

# print the final string
print(output)
