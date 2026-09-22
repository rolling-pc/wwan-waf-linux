/*
 * Copyright (C) 2021 MediaTek Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __TRANSPORT_PCIE__H
#define __TRANSPORT_PCIE__H

#include "fdevent.h"
#include "adb.h"
#include "sysdeps.h"

#define PCIE_SERIAL_UNKNOW  "0123456mediatek"

struct pcie_info {
	unsigned short device;
	unsigned short vendor;
	unsigned int class;
};

struct pcie_handle {
	int fd;
	struct pcie_info *info;
    int debug;

	adb_cond_t notify;
	adb_mutex_t lock;

	int (*write)(struct pcie_handle *h, const void *data, int len);
	int (*read)(struct pcie_handle *h, void *data, int len);
	void (*kick)(struct pcie_handle *h);
};

#endif
