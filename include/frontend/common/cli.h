/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>

/* A string takes a required argument, a number takes an optional argument,
 * and a flag records that the option appeared. */
typedef struct {
    char key;
    const char* name;
    const char* help;
    const char** argument;
    bool* flag;
    unsigned long* number;
    bool optional_bool; /* --flag=0 clears the flag; short form takes no value */
} frontend_cli_option_t;

int frontend_configure(int argc, char* argv[], const frontend_cli_option_t* options, size_t count);
