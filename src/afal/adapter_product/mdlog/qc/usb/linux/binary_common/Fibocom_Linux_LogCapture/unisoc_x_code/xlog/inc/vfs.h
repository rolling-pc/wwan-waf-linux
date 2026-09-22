/* Copyright (C) 2018 RDA Technologies Limited and/or its affiliates("RDA").
 * All rights reserved.
 *
 * This software is supplied "AS IS" without any warranties.
 * RDA assumes no responsibility or liability for the use of the software,
 * conveys no license or title under any patent, copyright, or mask work
 * right to the product. RDA reserves the right to make changes in the
 * software without notification.  RDA also make no representation or
 * warranty that such application will be suitable for the specified use
 * without further testing or modification.
 */

#ifndef _VFS_H_
#define _VFS_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/file.h>
#include "dirent.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VFS_PATH_MAX (192)

int vfs_rmchildren(const char *path);

/**
 * delete directory and children under the directory
 *
 * \param [in] path     directory path
 * \return
 *      - the number of bytes read on success
 *      - -1 on error
 */
int vfs_rmdir_recursive(const char *path);

/**
 * recursive total file size of a directory
 *
 * \param [in] path directory name
 * \return
 *      - recursive total file size of a directory
 *      - -1 on error, invalid parameter or out of memory
 */
int64_t vfs_dir_total_size(const char *path);

#ifdef __cplusplus
}
#endif
#endif // H
