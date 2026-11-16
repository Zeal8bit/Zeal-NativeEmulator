/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "hw/zeal.h"
int frontend_init(zeal_t*, bool allow_debugger);
void frontend_open(bool debug);
void frontend_bind_input(zeal_t*);
int  frontend_bind_output(zeal_t*);
void frontend_reset_input(zeal_t*);
int  frontend_run(zeal_t*);
void frontend_close(zeal_t*);

int frontend_configure(int argc, char* argv[]);
void frontend_deinit(zeal_t*);
