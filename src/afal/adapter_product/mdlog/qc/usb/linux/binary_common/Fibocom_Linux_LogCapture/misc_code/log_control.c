#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "log_control.h"

/**
 * log_storage_control - Control the total log storage size by cyclically deleting log files in the directory
 * @file_name: Name of the log file just created
 * @max_file_num: The maximum number of files allowed to be reserved in the current directory
 * @is_start: Turn on or off the log storage limit, and request or release some memory
 *
 * Returns 0,representing normal, and other values represent abnormal.
 */
int log_storage_control(char *file_name, int max_file_num, int is_start)
{
    int i;
    int is_usable=0;
    int rm_index=0;
    static log_file_t *log_file_struct = NULL;
    static int s_current_index = 0;

    if(is_start == 0)
    {
        if(log_file_struct != NULL)
        {
            free(log_file_struct);
            log_file_struct = NULL;
        }
        LogInfo("free mem\r\n");
        return 0;
    }

    if(max_file_num <= 0)
        return 0;

    if(log_file_struct == NULL)
    {
        log_file_struct = (log_file_t *) malloc(sizeof(log_file_t) * max_file_num);
        if(log_file_struct == NULL)
        {
            LogInfo("malloc error\r\n");
            return -1;
        }
        for(i=0; i< max_file_num; i++)
        {
            log_file_struct[i].is_used = 0;
            log_file_struct[i].index = i;
            memset(log_file_struct[i].file_name, 0, NAME_BUF_SIZE);
        }
    }

    for(i=0; i < max_file_num; i++)
    {
        if(log_file_struct[i].is_used == 0)
        {
            is_usable = 1;
            s_current_index = i;
            break;
        }
    }

    if(is_usable == 0)
    {
        rm_index = ((s_current_index + 1) % max_file_num);
        LogInfo("rm index is %d, current index is %d, max file num is %d\r\n",rm_index, s_current_index, max_file_num);
        if(unlink(log_file_struct[rm_index].file_name) < 0)
        {
            LogInfo("unlink error\r\n");
        }
        LogInfo("rm log file name %s\r\n", log_file_struct[rm_index].file_name);
        s_current_index = rm_index;
    }

    strcpy(log_file_struct[s_current_index].file_name, file_name);
    log_file_struct[s_current_index].is_used = 1;

    LogInfo("log storage control end\r\n");
    return 0;
}
