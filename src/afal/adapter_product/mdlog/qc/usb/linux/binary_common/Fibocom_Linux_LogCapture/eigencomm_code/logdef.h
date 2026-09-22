#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <pthread.h>
#include <asm/ioctls.h>
#define termios asmtermios
#include <asm-generic/termbits.h>
#undef  termios
#include <unistd.h>     // UNIX standard function definitions
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
#include <sys/stat.h>
#include <sys/time.h>

enum RamDumpErrorType {
  UE_OK = 0,
  UE_TIMEOUT_ERR,
  UE_NOT_COMMAND_COMM_ERR,
  UE_DEVICE_NOT_CONNECT

};

typedef struct _SYSTEMTIME {
    uint16_t Year;
    uint16_t Month;
    uint16_t DayOfWeek;
    uint16_t Day;
    uint16_t Hour;
    uint16_t Minute;
    uint16_t Second;
    uint16_t Milliseconds;
} SYSTEMTIME;


extern int catch_dump_proc(/*int fd, uint8_t* pbuf, size_t size*/int usb_mode);
extern size_t get_data(uint8_t* pBuf);
extern void clear_data();
extern size_t write_uart_binary(int fd, uint8_t* pbuf, size_t size);
//extern size_t read_uart_binary(int fd, uint8_t* pbuf, size_t size);
extern char *get_time_name(int hasms);
extern int create_trace_log();
extern void trace_log_print(char* fmt);
extern void trace_log_close(int fd);
extern int trace_log_fd;
extern int dump_uart_fd;
extern size_t trace_write(int fd, const void *buf, size_t size);

#define trace_log(fmt, arg... ) \
do {\
  char szlog[1024] = {0}; \
  snprintf(szlog, sizeof(szlog), "[%s] " fmt,  get_time_name(1), ## arg); \
  printf("%s", szlog); \
  } \
  while (0)
