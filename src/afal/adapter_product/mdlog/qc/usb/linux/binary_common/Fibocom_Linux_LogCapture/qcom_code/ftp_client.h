#ifndef __FTP_CLIENT_H
#define __FTP_CLIENT_H

#include <arpa/inet.h>

extern volatile int ftp_put_flag;
extern char g_ftp_put_filename[];

int ftp_main(int argc, char *argv[]);

#endif
