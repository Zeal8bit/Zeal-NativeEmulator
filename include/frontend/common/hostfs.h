/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "hw/hostfs.h"

void frontend_hostfs_bind(zeal_hostfs_t* core);
void frontend_hostfs_close(void);
