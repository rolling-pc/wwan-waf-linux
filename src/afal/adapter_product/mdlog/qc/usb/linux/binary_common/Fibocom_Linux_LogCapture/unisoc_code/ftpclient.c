#include<stdio.h>
#include<sys/types.h>//socket
#include<sys/socket.h>//socket
#include<stdlib.h>//sizeof
#include<unistd.h>
#include<arpa/inet.h>
#include<string.h>
#include<ctype.h>//isdigit()
#include<fcntl.h>//open()
#include<dirent.h>
#include<sys/stat.h>//stat()
#include<grp.h>
#include<pwd.h>
#include<time.h>
#include <errno.h>
#include<sys/sendfile.h>

#define PORT 21
#define MAXSZ 128
#define MAXSZ_2 256

#define MIN_VALUE -1
#define INITIALISE 0
#define MIN_IP 0
#define MAX_IP 255

#define bzero(buffer, size) memset(buffer, 0, size)

char passiver[]="PASV\r\n";

/*
Generate a passive PORT number from the variables sent by server.
*/
int passive_port_number(char *message)
{
    int i = INITIALISE;
    int count = INITIALISE;
    int port = INITIALISE;
    char *token;
    char delim[]=" ,)";/* Delimiters for strtok()*/

    while(message[i] != '\0' && count < 4)/* To reach to th first PORT variable */
    {
        if(message[i] == ',')
        {
            count++;
        }
        i++;
    }

    count = 0;

    token = strtok(message + i,delim);
    while(token != NULL)
    {
        if(isdigit(token[0]))
        {
            if(count == 1)
            {
                port += atoi(token);
            }

            if(count == 0)
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
int func_to_connect_passive(char *address,int port)
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

ssize_t do_sendfile(int out_fd, int in_fd, off_t offset, size_t count) {
    ssize_t bytes_sent;
    size_t total_bytes_sent = 0;
    while (total_bytes_sent < count) {
        if ((bytes_sent = sendfile(out_fd, in_fd, &offset,
                count - total_bytes_sent)) <= 0) {
            if (errno == EINTR || errno == EAGAIN) {
                // Interrupted system call/try again
                // Just skip to the top of the loop and try again
                continue;
            }
            perror("sendfile");
            return -1;
        }
        total_bytes_sent += bytes_sent;
    }
    return total_bytes_sent;
}

/*
Upload files on server.
*/
void put_content(char *arg,char *user_input,int sockfd)
{
    /* Temporary variables*/
    int no_of_bytes;
    int port;
    int newsockfd;
    int fd = -1;
//    int p;
//    int total;
    int size;
//    int file_size;
//    int temp;

    struct timeval tm;/* time structure to set time wait for receive buffer */
    tm.tv_sec = 1;
    tm.tv_usec = 750000;

    struct stat buff;
    off_t offset = 0;
//    int sent_bytes = 0;
    char message_from_server[MAXSZ];
    char message_to_server[MAXSZ];
    char file[MAXSZ];// File name
    char file_name[MAXSZ];// File name with instruction to server
    char data[MAXSZ];// Data transfer
    /* Initialise all the character arrays */
    bzero(message_from_server,MAXSZ);
    bzero(message_to_server,MAXSZ);
    bzero(file_name,MAXSZ);
    bzero(file,MAXSZ);
    bzero(data,MAXSZ);

    /* Tell server to change to BINARY mode */
    send(sockfd,"TYPE I\r\n",8,0);
    printf("#####[%s] [%s]\n",arg,user_input);

    while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0)
    {
        message_from_server[no_of_bytes] = '\0';
        printf("%s",message_from_server);
        if(strstr(message_from_server,"200 ") > 0 || strstr(message_from_server,"501 ") > 0 ||strstr(message_from_server,"500 ") > 0 ||strstr(message_from_server,"504 ") > 0 ||strstr(message_from_server,"421 ") > 0 || strstr(message_from_server,"530 ") > 0)
            break;
    }
    printf("\n");

    if(strstr(message_from_server,"501 ") > 0 ||strstr(message_from_server,"500 ") > 0 ||strstr(message_from_server,"504 ") > 0 ||strstr(message_from_server,"421 ") > 0 || strstr(message_from_server,"530 ") > 0)
        return;

    /* Send request for PASSIVE connection */
    send(sockfd,passiver,strlen(passiver),0);

    while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0)
    {
        message_from_server[no_of_bytes] = '\0';
        printf("%s",message_from_server);
        if(strstr(message_from_server,"227 ") > 0 || strstr(message_from_server,"501 ") > 0 ||strstr(message_from_server,"500 ") > 0 ||strstr(message_from_server,"502 ") > 0 ||strstr(message_from_server,"421 ") > 0 || strstr(message_from_server,"530 ") > 0)
            break;
    }
    printf("\n");

    if(strstr(message_from_server,"501 ") > 0 ||strstr(message_from_server,"500 ") > 0 ||strstr(message_from_server,"502 ") > 0 ||strstr(message_from_server,"421 ") > 0 || strstr(message_from_server,"530 ") > 0)
        return;

    /* Server accepts request and sends PORT variables */
    if(strncmp(message_from_server,"227",3) == 0)
    {
        /* Generate a PORT number using PORT variables */
        port = passive_port_number(message_from_server);

        /* Connect to server using another PORT for file transfers */
        newsockfd = func_to_connect_passive(arg,port);
        fcntl(newsockfd,F_SETFL,O_NDELAY);

        /* Send file name to server */
        sprintf(file_name,"STOR %s\r\n",user_input + 4);
        send(sockfd,file_name,strlen(file_name),0);

        while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0)
        {
            message_from_server[no_of_bytes] = '\0';
            printf("%s",message_from_server);
            if(strstr(message_from_server,"550 ") > 0 ||strstr(message_from_server,"125 ") > 0 ||strstr(message_from_server,"150 ") > 0 || strstr(message_from_server,"501 ") > 0 ||strstr(message_from_server,"500 ") > 0 ||strstr(message_from_server,"452 ") > 0 ||strstr(message_from_server,"421 ") > 0 || strstr(message_from_server,"530 ") > 0 || strstr(message_from_server,"553 ") > 0 ||strstr(message_from_server,"532 ") > 0)
                break;
        }
        printf("\n");

        /* Send file data to server */
        if(strncmp(message_from_server,"150",3) == 0 || strncmp(message_from_server,"125",3) == 0)
        {
            sprintf(file,"%s",user_input + 4);

            fd = open(file,O_RDONLY);
            if(fd == -1)
            {
                close(newsockfd);
                printf("###open file error\n");
                return;
            }
            fstat(fd,&buff);
            size = (int)buff.st_size;

            printf("Uploading [");
            printf("#### size =%d",size);
            do_sendfile(newsockfd, fd, offset, size);
            printf("] 100%%\n");

            close(newsockfd);
            close(fd);
            remove(file);
            /* Set time boundation on receive buffer */
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(char *)&tm,sizeof(tm));
            while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0)
            {
                message_from_server[no_of_bytes] = '\0';
                printf("%s\n",message_from_server);
                if(strstr(message_from_server,"226 ") > 0)
                    break;
            }
        }

    }
}

/*Get home directory of user executing program */
char *find_home_dir(char *file)
{
    struct passwd *pw;
    char *sudo_uid = getenv("SUDO_UID");
    pw = getpwuid(atoi(sudo_uid));
    printf("file is %s\r\n", file);
    return pw->pw_dir;

}

/*Validating IP Address*/
int validate_ip(char *ip)
{
    int value_1 = MIN_VALUE;
    int value_2 = MIN_VALUE;
    int value_3 = MIN_VALUE;
    int value_4 = MIN_VALUE;
    int count = INITIALISE;
    int i = INITIALISE;

    while(ip[i] != '\0')
    {
        if(ip[i] == '.')
            count++;
        i++;
    }

    if(count != 3 )
        return -1;
    else
    {
        sscanf(ip,"%d.%d.%d.%d",&value_1,&value_2,&value_3,&value_4);

        if(value_1 < MIN_IP || value_2 < MIN_IP || value_3 < MIN_IP || value_4 < MIN_IP || value_1 > MAX_IP || value_2 > MAX_IP || value_3 > MAX_IP || value_4 > MAX_IP)/* IP Addresses from 0.0.0.0 to 255.255.255.255*/
            return -1;
        else
            return 1;

    }

}
;
extern char g_str_sub_unisoc_log_dir[];//Modify for MBB0080-496 20230615 zhangboxing
extern char g_str_unisoc_log_dir[];
extern char g_ftpfilename[];
extern int ftp_put_unisoc_flag;
static int mkd_flag = 1;
void mssleep(long msec)
{
        long long us = msec;
        us *= 1000;
        usleep(us);
}
int ftp_unisoc_main(int argc,char *argv[])
{
    int sockfd;/* to create socket */
    int no_of_bytes;/* number of bytes sent or received from server */

    /*Temporary Variables*/
    int connect_value;
    int ip_valid;
    int temp = MIN_VALUE;
//    int count;
//    int dir_check;

//    clock_t start,end;
//    double cpu_time;

    struct sockaddr_in serverAddress;/* client will connect on this */

    char message_from_server[MAXSZ];/* message from server*/
    char user_input[MAXSZ];/* input from user */
    char message_to_server[MAXSZ];/* message to server */
    char user[MAXSZ_2];/* user details sent to server */
    char pass[MAXSZ_2];/* password details sent to server */
    char dir[MAXSZ];/* directory name */
    char username[MAXSZ];/* username entered by the user */
//    char *home_dir;
    char password[MAXSZ];/* password enterd by user */
    if(argc != 4) /* `./executable ip-adddress` */
    {
        printf("Error: argument should be ip-address of server\n");
        return -1;
    }

    ip_valid = validate_ip(argv[1]);/* Validate ip-address entered by user */

    if(ip_valid == MIN_VALUE)/* Invalid ipaddress */
    {
        printf("Error: Invalid ip-address\n");
        return -1;
    }
    snprintf(username, MAXSZ, "%s", argv[2]);
    snprintf(password, MAXSZ, "%s", argv[3]);
    /*home_dir = find_home_dir(argv[0]); Home directory of user executing the program */
    sprintf(user,"USER %s\r\n",username);
    sprintf(pass,"PASS %s\r\n",password);
    /* Infinite Loop for user operation */
    while(1)
    {
        temp = 0;
        if( ftp_put_unisoc_flag != 1)
        {
             mssleep(500);
             continue;
        }
        
        sockfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);/* Create socket */

        if(sockfd == -1)/* Error in socket creation */
        {
            perror("Error"); 
            continue;
        }

        bzero(&serverAddress,sizeof(serverAddress));/* Initialise structure */

        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = inet_addr(argv[1]);
        serverAddress.sin_port = htons(PORT);

        /* Connect to server */
        connect_value = connect(sockfd,(struct sockaddr *)&serverAddress,sizeof(serverAddress));
        if(connect_value == -1)/* Connection Error */
        {
            perror("Error");
            close(sockfd);
            continue;
        }

        printf("Connected to %s.\n",argv[1]);

        bzero(message_from_server,MAXSZ);
        /* Receive message from server "Server will send 220" */
        while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0 )
        {
            message_from_server[no_of_bytes] = '\0';
            printf("%s\n",message_from_server);

            if(strstr(message_from_server,"220 ") > 0 || strstr(message_from_server,"421 ") > 0)
                break;
        }

        if(strstr(message_from_server,"421 ") > 0)
        {
            close(sockfd);
            continue;
        }

        send(sockfd,user,strlen(user),0);/* Send username to server */


        /*
        Receive message from server after sending user name.
        Message with code 331 asks you to enter password corresponding to user.
        Message with code 230 means no password is required for the entered username(LOGIN successful).
        */
        bzero(message_from_server,MAXSZ);
        while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0)
        {
        message_from_server[no_of_bytes] = '\0';
        if(strncmp(message_from_server,"331",3) == 0)
        {
            temp = 1;
        }

        if(strncmp(message_from_server,"230",3) == 0)
        {
            temp = 2;
        }

        if(strncmp(message_from_server,"530",3) == 0)
        {
            temp = 0;
        }
        printf("%s\n",message_from_server);
        if(strstr(message_from_server,"230 ") > 0 || strstr(message_from_server,"500 ") > 0 || strstr(message_from_server,"501 ") > 0 || strstr(message_from_server,"421 ") > 0 || strstr(message_from_server,"332 ") > 0 || strstr(message_from_server,"530 ")|| strstr(message_from_server,"331 ") > 0)
            break;
        }

        if(temp == 1)
        {
            send(sockfd,pass,strlen(pass),0);/* Send password to server */

            /* Receive message from server */
            bzero(message_from_server,MAXSZ);
            while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0)
            {
                message_from_server[no_of_bytes] = '\0';

                if(strncmp(message_from_server,"230",3) == 0)
                {
                    temp = 2;
                }

                if(strncmp(message_from_server,"530",3) == 0)
                {
                    temp = 0;
                }
                printf("%s\n",message_from_server);

                if(strstr(message_from_server,"230 ") > 0 || strstr(message_from_server,"500 ") > 0 || strstr(message_from_server,"501 ") > 0 || strstr(message_from_server,"421 ") > 0 || strstr(message_from_server,"332 ") > 0 || strstr(message_from_server,"530 ")|| strstr(message_from_server,"503 ") > 0 || strstr(message_from_server,"202 ") > 0)
                    break;
            }
        }

        if(temp == 0)
        {
            close(sockfd);
            continue;
        }

        bzero(message_to_server,MAXSZ);
        bzero(message_from_server,MAXSZ);
        /* Systen type(Server) */
        sprintf(message_to_server,"SYST\r\n");
        send(sockfd,message_to_server,strlen(message_to_server),0);
        while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0)
        {
            message_from_server[no_of_bytes] = '\0';
            printf("%s\n",message_from_server);
            if(strstr(message_from_server,"215 ") > 0 || strstr(message_from_server,"500 ") > 0 || strstr(message_from_server,"501 ") > 0 || strstr(message_from_server,"421 ") > 0 || strstr(message_from_server,"502 ") > 0) 	
                break;

        }

        /* Change directory on client side */
        if(chdir(g_str_unisoc_log_dir) == 0)
        {
            printf("Directory [%s ]successfully changed\n\n",g_str_unisoc_log_dir);
        }
        else
        {
            perror("Error");
        }

        if(mkd_flag == 1)
        {
            /* Initialise strings */
            bzero(message_to_server,MAXSZ);
            bzero(message_from_server,MAXSZ);
            /* Creating diectory on server */
            sprintf(message_to_server,"MKD %s\r\n",g_str_sub_unisoc_log_dir);
            send(sockfd,message_to_server,strlen(message_to_server),0);
            while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0 )
            {
                message_from_server[no_of_bytes] = '\0';
                if(strncmp(message_from_server,"550",3) == 0)/* MKD fails*/
                {
                    printf("Error: Creating directory failed.\n\n");
                }
                else
                    printf("%s\n",message_from_server);

                if(strstr(message_from_server,"257 ") > 0 || strstr(message_from_server,"530 ") > 0 || strstr(message_from_server,"500 ") > 0|| strstr(message_from_server,"501 ") > 0 || strstr(message_from_server,"421 ") > 0 || strstr(message_from_server,"502 ") > 0 || strstr(message_from_server,"550 ") > 0)
                    break;
            }
            mkd_flag = 0;
        }

        /* Change directory on server side */
        sprintf(dir,"CWD %s\r\n",g_str_sub_unisoc_log_dir);
        send(sockfd,dir,strlen(dir),0);

        /* Initialise strings */
        bzero(message_from_server,MAXSZ);

        while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0)
        {
            message_from_server[no_of_bytes] = '\0';
            printf("%s\n",message_from_server);

            if(strstr(message_from_server,"530 ") > 0 || strstr(message_from_server,"250 ") > 0 || strstr(message_from_server,"500 ") > 0 || strstr(message_from_server,"501 ") > 0 || strstr(message_from_server,"421 ") > 0 || strstr(message_from_server,"502 ") > 0 || strstr(message_from_server,"550 ") > 0)
                break;
        }

    printf("#######ready ftp uploade %s\n",g_ftpfilename);

    sprintf(user_input, "put %s", g_ftpfilename);
    put_content(argv[1],user_input,sockfd);
    ftp_put_unisoc_flag = 0;

    bzero(message_to_server,MAXSZ);
    bzero(message_from_server,MAXSZ);
    sprintf(message_to_server,"QUIT\r\n");
    /* Send message to server */
    send(sockfd,message_to_server,strlen(message_to_server),0);
    while((no_of_bytes = recv(sockfd,message_from_server,MAXSZ,0)) > 0)
    {
        message_from_server[no_of_bytes] = '\0';
        printf("%s\n",message_from_server);
        if(strstr(message_from_server,"221 ") > 0 || strstr(message_from_server,"500 ") > 0)
            break;
    }
    close(sockfd);
    }
    //close(sockfd);
    return 0;
}
