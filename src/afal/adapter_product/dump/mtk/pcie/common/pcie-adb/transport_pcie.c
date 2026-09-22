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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysdeps.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "transport_pcie.h"

#define  TRACE_TAG  TRACE_TRANSPORT
#include "adb.h"

#ifdef HAVE_BIG_ENDIAN
#define H4(x)	(((x) & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | (((x) & 0x000000FF) << 24)
static inline void fix_endians(apacket *p)
{
    p->msg.command     = H4(p->msg.command);
    p->msg.arg0        = H4(p->msg.arg0);
    p->msg.arg1        = H4(p->msg.arg1);
    p->msg.data_length = H4(p->msg.data_length);
    p->msg.data_check  = H4(p->msg.data_check);
    p->msg.magic       = H4(p->msg.magic);
}
unsigned host_to_le32(unsigned n)
{
    return H4(n);
}
#else
#define fix_endians(p) do {} while (0)
unsigned host_to_le32(unsigned n)
{
    return n;
}
#endif

static void dump_pure_data(const unsigned char *buf, int size)
{
    unsigned char *p = (unsigned char *)buf;
    int i;

    adb_mutex_lock(&D_lock);
    for (i = 0; i < size; i++) {
        if (!(i % 4))
            fprintf(stderr, "\nbyte[%2d - %2d] = ", i, (i + 3) > size ? size : (i + 3));
        fprintf(stderr, "%8x\t", *p++);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "\n");
    fflush(stderr);
    adb_mutex_unlock(&D_lock);
}

static int remote_read(apacket *p, atransport *t)
{
    struct pcie_handle *pcie = t->pcie;
    int ret;

    ret = unix_read(pcie->fd, &p->msg, sizeof(amessage));
    if (ret < 0) {
        D("remote pcie: read terminated (message)\n");
        D("ret = %d\n", ret);
        return ret;
    }

    fix_endians(p);

    if (ADB_TRACING && pcie->debug) {
        D(">***************start_rh1, data size = %ld************<\n", sizeof(amessage));
        dump_pure_data((const unsigned char *)&p->msg, sizeof(amessage));
        D(">***************end_rh1, data size = %ld************<\n", sizeof(amessage));
    }

    if(check_header(p)) {
        D("remote pcie: check_header failed\n");
        D("ret = %d\n", ret);
        return -1;
    }

    if(p->msg.data_length) {
        ret = unix_read(pcie->fd, p->data, p->msg.data_length);
        if (ret < 0) {
            D("remote pcie: terminated (data)\n");
            return ret;
        }

        if (ADB_TRACING && pcie->debug) {
            D(">***************start_rd1, data size = %d************<\n", p->msg.data_length);
            dump_pure_data(p->data, p->msg.data_length);
            D(">***************end_rd1, data size = %d************<\n", p->msg.data_length);
        }
    }

    if(check_data(p)) {
        D("remote pcie: check_data failed\n");
        return -1;
    }

    return 0;
}

static int remote_write(apacket *p, atransport *t)
{
    unsigned size = p->msg.data_length;
    struct pcie_handle *pcie = t->pcie;
    int ret;

    fix_endians(p);

    if (ADB_TRACING && pcie->debug) {
        D(">***************start_wh1, data size = %ld************<\n", sizeof(amessage));
        dump_pure_data((const unsigned char *)&p->msg, sizeof(amessage));
        D(">***************end_wh1, data size = %ld************<\n", sizeof(amessage));
    }

    ret = unix_write(pcie->fd, &p->msg, sizeof(amessage));
    if (ret <= 0) {
        D("remote pcie: 1 - write terminated\n");
        D("ret = %d\n", ret);
        return ret;
    }

    if(p->msg.data_length == 0)
        return 0;

    if (ADB_TRACING && pcie->debug) {
        D(">***************start_wd1, data size = %d************<\n", p->msg.data_length);
        dump_pure_data(p->data, p->msg.data_length);
        D(">***************end_wd1, data size = %d************<\n", p->msg.data_length);
    }

    ret = unix_write(pcie->fd, p->data, size);
    if (ret <= 0) {
        D("remote pcie: 2 - write terminated\n");
        D("ret = %d\n", ret);
        return ret;
    }

    return 0;
}

static void remote_close(atransport *t)
{
	struct pcie_handle *pcie = t->pcie;

	D("remote close\n");
	unix_close(pcie->fd);
	pcie->fd = -1;
}

static void remote_kick(atransport *t)
{
	struct pcie_handle *pcie = t->pcie;

	if (pcie->kick)
		pcie->kick(pcie);
}

void init_pcie_transport(atransport *t, struct pcie_handle *h, const char *devpath, int state)
{
	/* On host side, we need open the file here */
	if (h->fd < 0)
		h->fd = unix_open(devpath, O_RDWR);

	t->close = remote_close;
	t->kick = remote_kick;
	t->read_from_remote = remote_read;
	t->write_to_remote = remote_write;
	t->sync_token = 1;
	t->connection_state = state;
	t->type = kTransportUsb;
	t->pcie = h;

	/* Device information */
	t->serial = strdup(PCIE_SERIAL_UNKNOW);
	t->device = strdup(PCIE_SERIAL_UNKNOW);
	t->product = strdup(PCIE_SERIAL_UNKNOW);
    t->model = strdup(PCIE_SERIAL_UNKNOW);

#if ADB_HOST
	HOST = 1;
#else
	HOST = 0;
#endif
}

#if ADB_HOST

#ifdef ADB_WINDOWS

#define PORT_VID		"8087"
#define PORT_PID		"0B63"
#define DEVICE_INFO_PATH	"/proc/registry/HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Enum/PCI"
// Begin
#define DEVICE_INSTANCE_PATH "/proc/registry/HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Services/WUDFRd/Enum"
// End

/* pcie_find_port
 *
 * find com port by walk the device tree of registry
 */
#if 0
static int pcie_find_port(const char *dir, const char *port, int depth)
{
	DIR *dp;
	struct dirent *entry;
	struct stat statbuf;

	int fd, ret = 0;
	char com_buf[10];
	char *ptr;

	if((dp = opendir(dir)) == NULL) {
		fprintf(stderr,"cannot open directory: %s\n", dir);
		return 0;
	}

	chdir(dir);

	while((entry = readdir(dp)) != NULL) {
		if (!strcmp(entry->d_name, port)) {

			lstat(entry->d_name, &statbuf);
			if(S_ISDIR(statbuf.st_mode)) {
				/* found a directory, but ignore . and .. */
				if(strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0)
					continue;

				ret = pcie_find_port(entry->d_name, "PortName", 1);
				if (ret != 0)
					break;
			} else {
				fd = unix_open(entry->d_name, O_RDONLY);
				if (fd < 0) {
					fprintf(stderr, "open file: %s failed\n", entry->d_name);
					break;
				}

				unix_read(fd, com_buf, 10);

				/* com_buf must be COMx, and we only need the numbers */
				ret = strtol(com_buf + 3, &ptr, 10);
				break;
			}
		}

		if (depth > 0) {
			lstat(entry->d_name, &statbuf);
			if (S_ISDIR(statbuf.st_mode)) {
				if(strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0)
					continue;
				ret = pcie_find_port(entry->d_name, port, 1);
				if (ret != 0)
					break;
			}
		}
	}

	chdir("..");
	closedir(dp);

	return ret;
}
#else
// Begin
static int pcie_find_port(const char *dir, const char *port)
{
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;

    int fd, ret = 0;
    char com_buf[10];
    char path_buf[255];
    char *path = NULL;    
    char *ptr;

    // Firstly, we need find out the device instance path
    D("finding the device instance path\n");
    if((dp = opendir(DEVICE_INSTANCE_PATH)) == NULL) {
        fprintf(stderr,"cannot open directory: %s\n", dir);
        return 0;
    }

    chdir(DEVICE_INSTANCE_PATH);

    while((entry = readdir(dp)) != NULL) {
        lstat(entry->d_name, &statbuf);
        if(S_ISDIR(statbuf.st_mode)) {
            D("found dir: %s\n", entry->d_name);
            continue;
        } else {
            D("found file: %s\n", entry->d_name);
            fd = unix_open(entry->d_name, O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "open file: %s failed\n", entry->d_name);
                break;
            }
        }

        unix_read(fd, path_buf, 255);
        path_buf[254] = 0;
        D("file contents: %s\n", path_buf);
        if (strstr(path_buf, port) != NULL){
            // Found the port
            path = strrchr(path_buf, '\\'); // find the last '\' in the string
            if (path != NULL){
                path ++;
                D("found the device instance path: %s\n", path);
                break;
            }
        }
        unix_close(fd);
    }

    chdir("..");
    closedir(dp);

    if (path == NULL){
        D("Can't find the device instance path\n");
        return 0;
    }

    // Since the "path" is limited to 255, the size 1024 must be enough, the overflow will not happen
    char full_path[1024];
    strcpy(full_path, dir);
    strcat(full_path, "\\");
    strcat(full_path, port);
    strcat(full_path, "\\");
    strcat(full_path, path);
    strcat(full_path, "\\");
    strcat(full_path, "Device Parameters\\");
    strcat(full_path, "PortName");

    D("full path: %s\n", full_path);

    fd = unix_open(full_path, O_RDONLY);
    if (fd < 0){
        fprintf(stderr, "Open file: %s failed\n", full_path);
        return 0;
    }

    unix_read(fd, com_buf, 10);
    com_buf[9] = 0;
    D("PortName = %s\n", com_buf);

    /* com_buf must be COMx, and we only need the numbers */
    ret = strtol(com_buf + 3, &ptr, 10);
    unix_close(fd);
    return ret;
}
// End
#endif

#endif /* ADB_WINDOS */

void pcie_host_init(void)
{
    struct pcie_handle *pcie;
    char target_port[30] = {0};
    int fd;
    const char *debug = getenv("ADB_PCIE_DATA_TRACE");

#ifdef ADB_WINDOWS
    char com_port_id[50] = {0};
    int num = 0;

    snprintf(com_port_id, 50, "VID_%s&PID_%s", PORT_VID, PORT_PID);
    num = pcie_find_port(DEVICE_INFO_PATH, com_port_id);
    if (num == 0) {
        // Begin - Jeffrey
        // Change from stdout to stderr because of the stdout buffering
        // And we need print this message when ADB_TRACE is not enabled, so here we don't use "D"
        fprintf(stderr, "* pcie-adb port [%s] not found\n", com_port_id);
        // End - Jeffrey
        return;
    }

    /* For Cygwin device node mapping:
     * COM1 -> /dev/ttyS0
     * COM2 -> /dev/ttyS1
     * COM3 -> /dev/ttyS2
     * ...
     * etc.
     */
    num--;
    snprintf(target_port, 30, "/dev/ttyS%d", num);
#else
    if (access(PCIE_ADB_PATH, R_OK | W_OK) != -1)
        snprintf(target_port, 30, "%s", PCIE_ADB_PATH);
    else if (access(PCIE_ADB_PATH_MAX_SPRING, R_OK | W_OK) != -1)
        snprintf(target_port, 30, "%s", PCIE_ADB_PATH_MAX_SPRING);
#endif
    if (target_port[0] == 0)
    {
        fprintf(stderr, "* can not find adb port");
        return;
    }
    else
    {
        fd = unix_open(target_port, O_RDWR);
        if (fd < 0) {
            // Begin - Jeffrey
            // Change from stdout to stderr because of the stdout buffering
            // And we need print this message when ADB_TRACE is not enabled, so here we don't use "D"
            fprintf(stderr, "* can not open adb port: %s, error code = %d\n", target_port, errno);
            // End - Jeffrey
            return;
        }
    }

    pcie = calloc(1, sizeof(*pcie));
    pcie->fd = fd;

    pcie->debug = 0;

    if (debug && !strcmp(debug, "1"))
        pcie->debug = 1;

    register_pcie_transport(pcie, target_port, 1);
}

#else /* !ADB_HOST */
static void *pcie_open_thread(void *x)
{
	struct pcie_handle *pcie = (struct pcie_handle *)x;
	int fd = -1;
	char *banner = "Start PCIe port\n";

	while (1) {
		adb_mutex_lock(&pcie->lock);
		while (pcie->fd != -1)
			adb_cond_wait(&pcie->notify, &pcie->lock);
		adb_mutex_unlock(&pcie->lock);

		do {
			fd = unix_open(PCIE_ADB_PATH, O_RDWR);
			if (fd < 0) {
				adb_sleep_ms(1000);
			}
		} while (fd < 0);

		pcie->fd = fd;
		adb_write(fd, banner, strlen(banner));

		/* Get PCIe config information */
		fd = unix_open(PCIE_INFO_ADB_PATH, O_RDONLY);
		if (fd > 0) {
			pcie->info = calloc(1, sizeof(struct pcie_info));
			if (pcie->info)
				unix_read(fd, pcie->info, sizeof(pcie->info));

			unix_close(fd);
		}

		register_pcie_transport(pcie, PCIE_ADB_PATH, 1);
	}

	// never gets here
	return 0;
}

static void pcie_adbd_kick(struct pcie_handle *h)
{
	adb_mutex_lock(&h->lock);
	unix_close(h->fd);
	h->fd = -1;

	adb_cond_signal(&h->notify);
	adb_mutex_unlock(&h->lock);
}

void pcie_init(void)
{
	adb_thread_t tid;
	struct pcie_handle *h;

	h = calloc(1, sizeof(*h));

	h->kick = pcie_adbd_kick;
	h->fd = -1;

	adb_cond_init(&h->notify, 0);
	adb_mutex_init(&h->lock, 0);

	D("[ pcie_init - starting thread ]\n");
	if (adb_thread_create(&tid, pcie_open_thread, h)) {
		fatal_errno("[ cannot create pcie thread ]\n");
	}
}
#endif
