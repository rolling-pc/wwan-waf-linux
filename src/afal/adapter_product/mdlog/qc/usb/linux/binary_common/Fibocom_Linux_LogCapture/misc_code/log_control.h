#ifndef __LOG_CONTROL_H
#define __LOG_CONTROL_H

#include "misc.h"

typedef struct
{
    int index;
    int is_used;
    char file_name[NAME_BUF_SIZE];
}log_file_t;


int log_storage_control(char *file_name, int max_file_num, int is_start);

#endif



