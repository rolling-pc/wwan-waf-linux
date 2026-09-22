#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <syslog.h>

#define BUFFER_SIZE 8192
#define MAX_FILE_SIZE 500*1024*1024

static    int fd = -1;
static    FILE *fp =NULL;

int creatNewLogFile(char* log_path, FILE** fp)
{
    char buffer[BUFFER_SIZE]={0};
    FILE *fp1 = NULL;
    char name[256] = {0};
    int result_f = -1;

    fp1 = popen("date +%Y_%m%d_%H%M%S_%N | tr -d '\n'","r");
    if(fp1 != NULL)
    {
	    sprintf(buffer,"%s/mr_dump_",log_path);
        while(fread(name,1,256,fp1) > 0)
        ;;
        name[strcspn(name, "\n")] = '\0';
        sprintf(buffer + strlen(buffer), "%s", name);
        printf("[Modemlog file] is :%s\n", buffer);
        memset(name, 0, 256);
    }

    *fp = fopen(buffer, "a+");
    if(*fp == NULL)
    {
        printf("ERROR: open file fail");
        fclose(*fp);
        return -1;
    }

    result_f = setvbuf(*fp, NULL, _IONBF, 0);
    if (result_f == 0)
    {
        printf("file buff set success\n");
    }
    else
    {
        printf("file buff set fail\n");
        return -1;
    }

    if (pclose(fp1) == -1) {
        printf("close popen error\n");
        return -1;
    }
    return 0;
}


int main(int argc, char **argv)
{
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read;
    FILE *fp1 = NULL;
    char name[256] = {0};
    struct stat dir_path;
    fd_set rset;
    size_t fileSize = 0;

    struct epoll_event epool_events;
    struct epoll_event ev;
    int ep_fd = epoll_create(1);
    int result = 0;

    printf("[%s]:Fibocom_Linux_DumpLog_V1.0.0.0\n", __func__);
    
    int sys_ret = -1;

    char log_path[BUFFER_SIZE] = "/var/log/FM350";
    int FLAG_LOG_PATH = 0;
    int i = 0;
    if (argc < 2)
    {
        printf("not set log path (-f), the log will save to /var/log/FM350/");
    }
    for (i = 1; i < argc ;)
    {
        printf("argc[%d] = %s\n",i,argv[i]);
        printf("argc[%d] = %s\n",i+1,argv[i+1]);
        if (0 == strcmp(argv[i], "-f"))
        {
            if(argc - i > 1)
            {
                if(!strstr(argv[i + 1],"-"))
                {
                    strncpy(log_path,argv[i + 1],strlen(argv[i + 1]));
                    FLAG_LOG_PATH = 1;
                    i++;
                }
                else
                {
                    strncpy(log_path,"/var/log/FM350",strlen("/var/log/FM350"));
                }
            }
            i++;
        }
    }

    fd = open("/dev/wwan0fastboot0", O_RDWR);
    if (fd == -1) {
        printf("Error: not open\n");
        exit(1);
    }

    ev.data.fd = fd;
    ev.events = EPOLLIN;
    epoll_ctl(ep_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);

    if(stat("/usr/local/fibocomtools/LOG/FM350", &dir_path) != 0)
    {
        printf("FM350 dir is not exist! Creating it!~\n");
        fp1 = popen("cd /usr/local/fibocomtools;mkdir -p /usr/local/fibocomtools/LOG/FM350;cd -","r");
        if(fp1 != NULL)
        {
            while(fread(name, 1, 256, fp1) > 0)
                ;;
            memset(name, 0, 256);
        }
    }

    result = creatNewLogFile(log_path, &fp);
    if (result != 0)
    {
        printf("create log file fail,");
    }

    memset(buffer, 0, 4096);
    printf("[%s]: Start collect dumplog.....\n", argv[0]);

    system("echo -n \"oem mrdump\" > /dev/wwan0fastboot0");
    sleep(2);

    while(1)
    {
        /*
        fileSize = ftell(fp);
        //printf("log file size %d\n",fileSize);
        // ����ļ���С������ֵ���������ļ�
        if (fileSize > MAX_FILE_SIZE)
        {
            printf("log file size %d\n", fileSize);
            fflush(fp);
            fclose(fp);
            fp = NULL;
            result = creatNewLogFile(log_path, &fp);
            if (result != 0)
            {
                printf("create log file fail,");
            }
        }
        */
        //write(epool_events.data.fd, "_CTS", strlen("_CTS") );
        system("echo -n \"_CTS\" > /dev/wwan0fastboot0");
        int ret = epoll_wait(ep_fd, &epool_events, 1, -1);
        if (ret == -1) {
            printf("epoll_wait error\n");
            exit(EXIT_FAILURE);
        }
        while(1)
        {
            bytes_read = read(epool_events.data.fd, buffer, BUFFER_SIZE);
            //printf(" _CTS read buff size = %d\n buffer = %s\n", bytes_read, buffer);
            if (bytes_read <= 4) {
                continue;
            }
            else
            {
                fwrite(buffer,sizeof(unsigned char), bytes_read, fp);
                memset(buffer, 0, BUFFER_SIZE);
                break;
            }
        }

        //write(epool_events.data.fd, "_FIN" ,strlen("_FIN") );
        system("echo -n \"_FIN\" > /dev/wwan0fastboot0");
        ret = epoll_wait(ep_fd, &epool_events, 1, -1);
        if (ret == -1) {
            printf("epoll_wait error\n");
            exit(EXIT_FAILURE);
        }

        while(1)
        {
            bytes_read = read(epool_events.data.fd, buffer, BUFFER_SIZE);
            //printf("_FIN read buff size = %d\n buffer = %s\n", bytes_read, buffer);
            if (bytes_read <= 0)
            {
                continue;
            }
            else
            {
                if (strstr(buffer, "_RTS"))
                {
                    //printf("_RTS continue\n");
                    break;
                }
                else if(strstr(buffer, "MRDUMP08_DONE") != NULL)
                {
                        printf("MRDUMP08_DONE");
                        return 0;
                }
                else if (strstr(buffer, "FAILunknown command") != NULL)
                {
                        printf("ERROR: FAIL unknown command");
                        return 0;
                    }
                }
        }
        memset(buffer, 0, BUFFER_SIZE);
    }

    return 0;
}