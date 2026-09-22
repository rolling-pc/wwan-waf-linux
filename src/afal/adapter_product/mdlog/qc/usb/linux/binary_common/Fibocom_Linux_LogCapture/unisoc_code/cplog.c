#include "unisoc_log_main.h"
#include "misc.h"
#include "list.h"

extern ulog_ops_t  ulog_ops;
extern int         logfile_fd;    //Use as cplogfile_fd in unisoc ecxxxu
extern unsigned ulog_exit_requested;

void* ulog_logfile_save_thread(void* arg)
{
    ssize_t wc = 0;
    struct Head *head = arg;
    struct Node *p = NULL;

    if(NULL == head)
    {
        LogInfo("head is NULL!\n");
        return NULL;
    }

    while(1)
    {
        //LogInfo("head->count is %d, ulog_exit_requested is %d\n", head->count, ulog_exit_requested);
        if(head->count > 0)
        {
            //save log to file
            p = head->first;
            wc = ulog_ops.logfile_save(logfile_fd, p->rbuf, p->size, p->logtype);

            deleteNode(head);
        }
        else
        {
            if(1 == ulog_exit_requested)
            {
                /* close file */
                ulog_ops.logfile_close(logfile_fd);
                break;
            }
            usleep(5);
        }
    }

    return NULL;
}