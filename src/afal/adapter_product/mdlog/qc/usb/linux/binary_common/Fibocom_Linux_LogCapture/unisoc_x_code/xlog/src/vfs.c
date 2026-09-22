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

#include "vfs.h"
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

#include <pthread.h>

int vfs_rmchildren(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
        return -1;

    struct dirent *ent = (struct dirent *)calloc(1, sizeof(struct dirent));
    char *child = (char *)malloc(VFS_PATH_MAX + 256);
    if (ent == NULL || child == NULL)
    {
        closedir(dir);
        free(ent);
        free(child);
        return -1;
    }

    struct dirent *rent = NULL;
    int res = 0;
    while (readdir_r(dir, ent, &rent) >= 0 && rent != NULL)
    {
        if (strcmp(rent->d_name, "..") == 0 || strcmp(rent->d_name, ".") == 0)
            continue;
        if (rent->d_type == DT_REG)
        {
            snprintf(child, VFS_PATH_MAX + 256 - 1, "%s/%s", path, rent->d_name);
            if (unlink(child) < 0)
            {
                res = -1;
                break;
            }
        }
        else if (rent->d_type == DT_DIR)
        {
            snprintf(child, VFS_PATH_MAX + 256 - 1, "%s/%s", path, rent->d_name);
            if (vfs_rmchildren(child) < 0 || rmdir(child) < 0)
            {
                res = -1;
                break;
            }
        }
    }

    closedir(dir);
    free(ent);
    free(child);
    return res;
}

int vfs_rmdir_recursive(const char *path)
{
    int res = vfs_rmchildren(path);
    if (res < 0)
        return res;

    return rmdir(path);
}

int64_t vfs_dir_total_size(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
        return -1;

    struct dirent *ent = (struct dirent *)calloc(1, sizeof(struct dirent));
    char *child = (char *)malloc(VFS_PATH_MAX + 256);
    if (ent == NULL || child == NULL)
    {
        closedir(dir);
        free(ent);
        free(child);
        return -1;
    }

    struct stat st;
    struct dirent *rent = NULL;
    int64_t total = 0;
    while (readdir_r(dir, ent, &rent) >= 0 && rent != NULL)
    {
        if (strcmp(rent->d_name, "..") == 0 || strcmp(rent->d_name, ".") == 0)
            continue;
        if (rent->d_type == DT_REG)
        {
            snprintf(child, VFS_PATH_MAX + 256 - 1, "%s/%s", path, rent->d_name);
            if (stat(child, &st) != 0)
                continue;

            total += st.st_size;
        }
        else if (rent->d_type == DT_DIR)
        {
            snprintf(child, VFS_PATH_MAX + 256 - 1, "%s/%s", path, rent->d_name);
            int64_t sub_total = vfs_dir_total_size(child);
            if (sub_total < 0)
                continue;

            total += sub_total;
        }
    }

    closedir(dir);
    free(ent);
    free(child);
    return total;
}
