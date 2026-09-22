#include "logdef.h"
#include "queue.h"

//QUEUE *g_log_queue;
int trace_log_fd;
pthread_t tid_tracelog = 0;


size_t trace_write(int fd, const void *buf, size_t size)
{
    if ( fd < 0 || size <= 0 || !buf)
        return -1;
    
    return write(fd, buf, size);
}

/*
static void* trace_log_proc(void* arg)
{
    QUEUE_ITEM *item = NULL;

    while (1)
    {
        while (g_log_queue->items) 
        {
            item = Get_Queue_Item(g_log_queue);
            if (item && item->sz > 0)
            {
                trace_write(item->fd, item->data, item->sz);
                printf("%s", (const char*)item->data);
            }

            Free_Queue_Item(item);
        }

        if (!g_log_queue->items)
            usleep(1000);
    }
    
    return NULL;
}
*/
int create_trace_log()
{
    trace_log_fd = -1;

    char logFileName[100] = {0};
    char cure_dir_path[256] = {0};
    char log_dir[262] = {0};

    snprintf(log_dir, sizeof(log_dir), "%s", "./tracelog");
    mkdir(log_dir, 0755);

    snprintf(logFileName, sizeof(logFileName), "tracelog_%s.txt",get_time_name(0));
    snprintf(cure_dir_path,sizeof(cure_dir_path),"%.155s/%.80s",log_dir,logFileName);
    printf("%s : log file path:%s\n", __func__, cure_dir_path);

    trace_log_fd = open(cure_dir_path, O_CREAT | O_RDWR | O_TRUNC, 0444);
    //if (trace_log_fd != -1)
    //    pthread_create(&tid_tracelog, NULL, trace_log_proc, NULL);
    //g_log_queue = Initialize_Queue();
    return trace_log_fd;
}


void trace_log_print(char* fmt)
{
    if ( !fmt || strlen(fmt) <= 0)
        return;
    
    int n = 0;
    n = write(trace_log_fd, fmt, strlen(fmt));
    if ( n == -1)
        printf("write log file failed!\n");
    return;
    //Add_Queue_Item(g_log_queue, fmt, strlen(fmt), trace_log_fd);
} 

void trace_log_close(int fd)
{
    //pthread_cancel(tid_tracelog);
    close(fd);
}
