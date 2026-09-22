#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include "ftp_client.h"
#include "qlog.h"

#define PORT_NUM    21
#define MAXSZ       128

#define MIN_IP 0
#define MAX_IP 255

#define bzero(buffer, size) memset(buffer, 0, size)

char *passive = "PASV\r\n";

/*
Generate a passive PORT number from the variables sent by server.
*/
static int passive_port_number(char *message)
{
    int i = 0;
    int count = 0;
    int port = 0;
    char *token;
    char delim[]=" ,)";/* Delimiters for strtok()*/

    while (message[i] != '\0' && count < 4)/* To reach to th first PORT variable */
    {
        if (message[i] == ',')
        {
            count++;
        }
        i++;
    }

    count = 0;

    token = strtok(message + i,delim);
    while (token != NULL)
    {
        if (isdigit(token[0]))
        {
            if (count == 1)
            {
                port += atoi(token);
            }

            if (count == 0)
            {
                port = atoi(token)*256;
                count++;
            }

        }
        token = strtok(NULL,delim);
    }
    return port;

}

/* Create PASSIVE socket and connect to server */
static int func_to_connect_passive(char *address,int port)
{
    int newsockfd;
    struct sockaddr_in new_serverAddress;

    newsockfd = socket(AF_INET,SOCK_STREAM,0);
    //memset(&new_serverAddress, 0, sizeof(new_serverAddress));
    bzero(&new_serverAddress,sizeof(new_serverAddress));

    new_serverAddress.sin_family = AF_INET;
    new_serverAddress.sin_addr.s_addr = inet_addr(address);
    new_serverAddress.sin_port = htons(port);

    connect(newsockfd,(struct sockaddr *)&new_serverAddress,sizeof(new_serverAddress));

    return newsockfd;
}

static ssize_t ftp_sendfile(int out_fd, int in_fd, off_t offset, size_t count)
{
    ssize_t total_bytes_sent = 0;

    while (total_bytes_sent < count)
    {
        int bytes_sent = sendfile(out_fd, in_fd, &offset, count - total_bytes_sent);
        if (bytes_sent <= 0) {
            if (errno == EINTR || errno == EAGAIN) {
                // Interrupted system call/try again
                // Just skip to the top of the loop and try again
                continue;
            }
            printf("%s sendfile failed. errno:%d(%s)\n", __func__, errno, strerror(errno));
            return -1;
        }
        total_bytes_sent += bytes_sent;
    }

    return total_bytes_sent;
}

/*
Upload files on server.
*/
static void put_content(char *arg,char *user_input,int sockfd)
{
    int rec_bytes = 0;
    int newsockfd, fd = -1;
    off_t offset = 0;
    char server_readbuf[MAXSZ] = {0};
    char file_name[MAXSZ] = {0};// File name with instruction to server

    struct timeval tm;/* time structure to set time wait for receive buffer */
    tm.tv_sec = 1;
    tm.tv_usec = 750000;

    /* Tell server to change to BINARY mode */
    send(sockfd,"TYPE I\r\n",8,0);
    printf("[%s]: [%s] [%s]\n", __func__, arg, user_input);

    while ((rec_bytes = recv(sockfd,server_readbuf,MAXSZ,0)) > 0)
    {
        server_readbuf[rec_bytes] = '\0';
        printf("%s",server_readbuf);
        if (strstr(server_readbuf,"200 ") || strstr(server_readbuf,"501 ") ||strstr(server_readbuf,"500 ")
            ||strstr(server_readbuf,"504 ") ||strstr(server_readbuf,"421 ") || strstr(server_readbuf,"530 "))
            break;
    }
    printf("\n");

    if (strstr(server_readbuf,"501 ") ||strstr(server_readbuf,"500 ") ||strstr(server_readbuf,"504 ")
            ||strstr(server_readbuf,"421 ") || strstr(server_readbuf,"530 "))
        return;

    /* Send request for PASSIVE connection */
    send(sockfd, passive, strlen(passive), 0);

    while ((rec_bytes = recv(sockfd,server_readbuf,MAXSZ,0)) > 0)
    {
        server_readbuf[rec_bytes] = '\0';
        printf("%s",server_readbuf);
        if (strstr(server_readbuf,"227 ") || strstr(server_readbuf,"501 ") ||strstr(server_readbuf,"500 ")
            ||strstr(server_readbuf,"502 ") ||strstr(server_readbuf,"421 ") || strstr(server_readbuf,"530 "))
            break;
    }
    printf("\n");

    if (strstr(server_readbuf,"501 ") ||strstr(server_readbuf,"500 ") ||strstr(server_readbuf,"502 ")
        || strstr(server_readbuf,"421 ") || strstr(server_readbuf,"530 "))
        return;

    /* Server accepts request and sends PORT variables */
    if (strncmp(server_readbuf,"227",3) == 0)
    {
        /* Generate a PORT number using PORT variables */
        int port = passive_port_number(server_readbuf);

        /* Connect to server using another PORT for file transfers */
        newsockfd = func_to_connect_passive(arg, port);
        fcntl(newsockfd,F_SETFL,O_NDELAY);

        /* Send file name to server */
        sprintf(file_name, "STOR %s\r\n", user_input + 4);
        send(sockfd, file_name, strlen(file_name),0);

        while ((rec_bytes = recv(sockfd,server_readbuf,MAXSZ,0)) > 0)
        {
            server_readbuf[rec_bytes] = '\0';
            printf("%s",server_readbuf);
            if (strstr(server_readbuf,"550 ") ||strstr(server_readbuf,"125 ") ||strstr(server_readbuf,"150 ") || strstr(server_readbuf,"501 ")
                ||strstr(server_readbuf,"500 ") ||strstr(server_readbuf,"452 ") ||strstr(server_readbuf,"421 ") || strstr(server_readbuf,"530 ")
                || strstr(server_readbuf,"553 ") ||strstr(server_readbuf,"532 "))
                break;
        }
        printf("\n");

        /* Send file data to server */
        if (strncmp(server_readbuf,"150",3) == 0 || strncmp(server_readbuf,"125",3) == 0)
        {
            struct stat file_stat;

            sprintf(file_name, "%s", user_input + 4);

            fd = open(file_name,O_RDONLY);
            if (fd == -1)
            {
                close(newsockfd);
                printf("###open file %s error\n", file_name);
                return;
            }
            fstat(fd,&file_stat);

            printf("Uploading [");
            printf("#### size: %d", (int)file_stat.st_size);
            ftp_sendfile(newsockfd, fd, offset, file_stat.st_size);
            printf("] 100%%\n");

            close(newsockfd);
            close(fd);
            remove(file_name);
            /* Set time boundation on receive buffer */
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(char *)&tm,sizeof(tm));
            while ((rec_bytes = recv(sockfd,server_readbuf,MAXSZ,0)) > 0)
            {
                server_readbuf[rec_bytes] = '\0';
                printf("%s\n",server_readbuf);
                if (strstr(server_readbuf,"226 "))
                    break;
            }
        }
    }
}

/* check IP */
static int validate_ip(char *ip)
{
    int ip1 = -1, ip2 = -1;
    int ip3 = -1, ip4 = -1;
    int i, count = 0;

    for(i=0; ip[i] != '\0'; i++) {
        if (ip[i] == '.') {
            count++;
        }
    }

    if (count != 3 ) {
        return -1;
    }

    sscanf(ip, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);
    if (ip1 < MIN_IP || ip2 < MIN_IP || ip3 < MIN_IP || ip4 < MIN_IP
        || ip1 > MAX_IP || ip2 > MAX_IP || ip3 > MAX_IP || ip4 > MAX_IP) /* IP Addresses from 0.0.0.0 to 255.255.255.255*/
    {
        return -1;
    }

    return 0;
}


int ftp_main(int argc, char *argv[])
{
    static int ftp_mkdir_first = 1;
    int sockfd = -1; /* to create socket */
    int rec_bytes = 0; /* number of bytes sent or received from server */
    int temp = -1;
    struct sockaddr_in serverAddress;
    char server_readbuf[MAXSZ] = {0}; /* message from server*/
    char server_sendbuf[MAXSZ] = {0}; /* message to server */
    char *ip_addr = NULL, *username = NULL, *password = NULL;

    if (argc != 4 || argv[1] == NULL || argv[2] == NULL || argv[3] == NULL)
    {
        printf("The input params is ERROR.\n");
        return -1;
    }

    ip_addr = argv[1];
    username = argv[2];
    password = argv[3];

    if (validate_ip(ip_addr))
    {
        printf("Error: Invalid ip-address\n");
        return -1;
    }

    while (1)
    {
        if (ftp_put_flag == 0)
        {
            usleep(500*1000);
            continue;
        }

        temp = 0;
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == -1)
        {
            printf("socket %s failed. errno:%d(%s)\n", ip_addr, errno, strerror(errno));
            continue;
        }

        bzero(&serverAddress, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = inet_addr(ip_addr);
        serverAddress.sin_port = htons(PORT_NUM);
        /* Connect to server */
        if (-1 == connect(sockfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)))
        {
            printf("connect %s failed. errno:%d(%s)\n", ip_addr, errno, strerror(errno));
            close(sockfd);
            sockfd = -1;
            continue;
        }
        printf("Connected to %s.\n",ip_addr);

        bzero(server_readbuf,MAXSZ);
        /* Receive message from server "Server will send 220" */
        while ((rec_bytes = recv(sockfd, server_readbuf, MAXSZ,0)) > 0 )
        {
            server_readbuf[rec_bytes] = '\0';
            printf("%s\n",server_readbuf);

            if (strstr(server_readbuf,"220 ") || strstr(server_readbuf,"421 "))
                break;
        }

        if (strstr(server_readbuf,"421 "))
        {
            close(sockfd);
            sockfd = -1;
            continue;
        }

        bzero(server_sendbuf, MAXSZ);
        sprintf(server_sendbuf, "USER %s\r\n", username);
        send(sockfd, server_sendbuf, strlen(server_sendbuf), 0); /* Send username to server */

        /*
        Receive message from server after sending username.
        Message with code 331 asks you to enter password corresponding to user.
        Message with code 230 means no password is required for the entered username(LOGIN successful).
        */
        bzero(server_readbuf,MAXSZ);
        while ((rec_bytes = recv(sockfd,server_readbuf,MAXSZ,0)) > 0)
        {
            server_readbuf[rec_bytes] = '\0';
            if (strncmp(server_readbuf, "331", 3) == 0)
            {
                temp = 1;
            }
            else if (strncmp(server_readbuf, "230", 3) == 0)
            {
                temp = 2;
            }
            else if (strncmp(server_readbuf, "530",3) == 0)
            {
                temp = 0;
            }
            printf("%s\n",server_readbuf);
            if (strstr(server_readbuf,"230 ") || strstr(server_readbuf,"500 ") || strstr(server_readbuf,"501 ") || strstr(server_readbuf,"421 ")
                || strstr(server_readbuf,"332 ") || strstr(server_readbuf ,"530 ") || strstr(server_readbuf,"331 ")) {
                break;
            }
        }

        if (temp == 1)
        {
            bzero(server_sendbuf, MAXSZ);
            sprintf(server_sendbuf, "PASS %s\r\n", password);
            send(sockfd, server_sendbuf, strlen(server_sendbuf), 0);/* Send password to server */

            /* Receive message from server */
            bzero(server_readbuf,MAXSZ);
            while ((rec_bytes = recv(sockfd,server_readbuf, MAXSZ, 0)) > 0)
            {
                server_readbuf[rec_bytes] = '\0';

                if (strncmp(server_readbuf, "230", 3) == 0)
                {
                    temp = 2;
                }
                else if (strncmp(server_readbuf, "530", 3) == 0)
                {
                    temp = 0;
                }
                printf("%s\n", server_readbuf);

                if (strstr(server_readbuf,"230 ") || strstr(server_readbuf,"500 ") || strstr(server_readbuf,"501 ") || strstr(server_readbuf,"421 ")
                    || strstr(server_readbuf,"332 ") || strstr(server_readbuf,"530 ")|| strstr(server_readbuf,"503 ") || strstr(server_readbuf,"202 ")) {
                    break;
                }
            }
        }

        if (temp == 0)
        {
            close(sockfd);
            sockfd = -1;
            continue;
        }

        bzero(server_sendbuf, MAXSZ);
        sprintf(server_sendbuf, "SYST\r\n");
        send(sockfd,server_sendbuf,strlen(server_sendbuf),0);

        bzero(server_readbuf, MAXSZ);
        while ((rec_bytes = recv(sockfd,server_readbuf,MAXSZ,0)) > 0)
        {
            server_readbuf[rec_bytes] = '\0';
            printf("%s\n",server_readbuf);
            if (strstr(server_readbuf,"215 ") || strstr(server_readbuf,"500 ") || strstr(server_readbuf,"501 ")
                || strstr(server_readbuf,"421 ") || strstr(server_readbuf,"502 ")) {
                break;
            }
        }

        /* Change directory on client side */
        if (chdir(g_qcom_logdir) == 0)
        {
            printf("chdir [%s] OK.\n", g_qcom_logdir);
        }
        else
        {
            printf("chdir [%s] failed. errno:%d(%s)\n", g_qcom_logdir, errno, strerror(errno));
        }

        if (ftp_mkdir_first == 1)
        {
            bzero(server_sendbuf, MAXSZ);
            /* Creating directory on server */
            sprintf(server_sendbuf,"MKD %s\r\n", g_str_sub_log_dir);
            send(sockfd,server_sendbuf,strlen(server_sendbuf),0);

            bzero(server_readbuf, MAXSZ);
            while ((rec_bytes = recv(sockfd,server_readbuf,MAXSZ,0)) > 0 )
            {
                server_readbuf[rec_bytes] = '\0';
                if (strncmp(server_readbuf,"550",3) == 0)/* MKD fails*/
                {
                    printf("Error: Creating directory failed.\n\n");
                }
                else {
                    printf("%s\n", server_readbuf);
                }

                if (strstr(server_readbuf,"257 ") || strstr(server_readbuf,"530 ") || strstr(server_readbuf,"500 ") || strstr(server_readbuf,"501 ")
                    || strstr(server_readbuf,"421 ") || strstr(server_readbuf,"502 ") || strstr(server_readbuf,"550 ")) {
                    break;
                }
            }
            ftp_mkdir_first = 0;
        }

        /* Change directory on server side */
        bzero(server_sendbuf, MAXSZ);
        sprintf(server_sendbuf, "CWD %s\r\n", g_str_sub_log_dir);
        send(sockfd, server_sendbuf, strlen(server_sendbuf),0);

        bzero(server_readbuf, MAXSZ);
        while ((rec_bytes = recv(sockfd,server_readbuf,MAXSZ,0)) > 0)
        {
            server_readbuf[rec_bytes] = '\0';
            printf("%s\n",server_readbuf);

            if (strstr(server_readbuf,"530 ") || strstr(server_readbuf,"250 ") || strstr(server_readbuf,"500 ") || strstr(server_readbuf,"501 ")
                || strstr(server_readbuf,"421 ") || strstr(server_readbuf,"502 ") || strstr(server_readbuf,"550 "))
                break;
        }

        printf("g_ftp_put_filename: %s\n", g_ftp_put_filename);
        bzero(server_sendbuf, MAXSZ);
        sprintf(server_sendbuf, "put %s", g_ftp_put_filename);
        put_content(ip_addr, server_sendbuf, sockfd);
        ftp_put_flag = 0;

        bzero(server_sendbuf, MAXSZ);
        sprintf(server_sendbuf, "QUIT\r\n");
        send(sockfd,server_sendbuf, strlen(server_sendbuf),0);

        bzero(server_readbuf, MAXSZ);
        while ((rec_bytes = recv(sockfd, server_readbuf, MAXSZ, 0)) > 0)
        {
            server_readbuf[rec_bytes] = '\0';
            printf("%s\n",server_readbuf);
            if (strstr(server_readbuf, "221 ") || strstr(server_readbuf, "500 "))
                break;
        }
        close(sockfd);
        sockfd = -1;
    }

    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }

    return 0;
}
