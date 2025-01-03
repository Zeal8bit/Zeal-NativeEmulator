#pragma once

#include <stddef.h>
#include <stdint.h>

uint8_t compute_checksum(char *buf, size_t len);

void hex_to_str(uint8_t *num, char *str, int bytes);

void str_to_hex(char *str, uint8_t *num, int bytes);

int unescape(char *msg, char *end);
