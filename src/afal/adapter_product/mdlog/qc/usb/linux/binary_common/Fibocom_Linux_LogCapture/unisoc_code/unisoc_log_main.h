#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 20)
#include <linux/usb/ch9.h>
#else
#include <linux/usb_ch9.h>
#endif
#include <linux/usbdevice_fs.h>

typedef unsigned int uint32_t;
#define TFTP_F "tftp:"
#define FTP_F  "ftp:"
#define QLOG_BUF_SIZE (4*1024)

//#ifndef MIN
//#define MIN(a, b)	((a) < (b)? (a): (b))
//#endif

#define qlog_raw_log(fmt, arg... ) do { unsigned msec = ulog_msecs(); printf("\r[%03u.%03u] " fmt,  msec/1000, msec%1000, ## arg); fflush(stdout);} while (0)
#define unused_result_write(_fd, _buf, _count) do { if (write(_fd, _buf, _count) == -1) {} } while (0)


typedef struct {
    int (*init_filter)(int fd, const char *conf);
    int (*clean_filter)(int fd);
    int (*logfile_create)(const char *logfile_dir, const char *logfile_suffix, unsigned logfile_seq);
    int (*logfile_init)(int logfd, unsigned logfile_seq);
    size_t (*logfile_save)(int logfd, const void *buf, size_t size, int logtype);
    int (*logfile_close)(int logfd);
} ulog_ops_t;

struct ql_usb_interface_info {
    uint8_t bInterfaceNumber;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t bNumEndpoints;
    uint8_t ep_in;
    uint8_t ep_out;
};

struct ql_usb_device_info {
    int idVendor;
    int idProduct;
    int bNumInterfaces;
    int busnum;
    int devnum;
    int major;
    int minor;
    int bcdDevice;
    int hardware; // 0 ~ usb, 'p' ~ pcie

    int dm_major;
    int dm_minor;
    char devname[32];
    char usbdevice_pah[256];
    char ttyDM[32];
    char ttyGENERAL[32]; //for all

    struct ql_usb_interface_info dm_intf;
    struct ql_usb_interface_info general_intf;
    int general_type;
};


int isLittleEndian();
unsigned short swapShort(unsigned short num);
unsigned int swapInt(unsigned int num);


extern ulog_ops_t unisoc_log_ops;
extern ulog_ops_t tty2tcp_log_ops;
extern ulog_ops_t tcp_client_log_ops;
extern int g_is_unisoc_chip;
extern int g_is_unisoc_exx00u_chip;
extern unsigned ulog_exit_requested;
extern unsigned g_rx_log_count;
extern int g_donot_split_logfile;
extern const char *g_tftp_server_ip;
extern const char *g_ftp_server_ip;
extern const char *g_ftp_server_usr;
extern const char *g_ftp_server_pass;
extern int tty2tcp_sockfd;
extern int g_unisoc_log_type;

extern unsigned ulog_msecs(void);
extern int unisoc_catch_dump(int usbfd, int ttyfd, const char *logfile_dir, int RX_URB_SIZE, const char* (*ulog_time_name)(int));
extern uint64_t ulog_le64 (uint64_t v64);
extern int ulog_avail_space_for_dump(const char *dir, long need_MB);
extern int find_unisoc_modules(void);
extern struct ql_usb_device_info s_usb_device_info[];
extern int ql_usbfs_open_interface(const struct ql_usb_device_info *usb_dev, int intf);
extern int ql_usbfs_read(int usbfd, int ep_in, void *pbuf, unsigned len);
extern int ql_usbfs_write(int usbfd, int ep_out, const void *pbuf, unsigned len);
extern int ql_usbfs_read(int usbfd, int ep_in, void *pbuf, unsigned len);
extern int ql_usbfs_write(int usbfd, int ep_out, const void *pbuf, unsigned len);
extern size_t kfifo_write(int idx, const void *buf, size_t size);
extern void kfifo_free(int idx);
extern int kfifo_alloc(int fd);
extern int kfifo_idx(int fd);
extern int m_bVer_Obtained_change(void);
extern ssize_t ulog_poll_write(int fd, const void *buf, size_t size, unsigned timeout_mesc);
extern ssize_t ulog_poll_read(int fd,  void *pbuf, size_t size, unsigned timeout_msec);
extern int ql_match_dm_device(const char *devname, struct ql_usb_device_info *ql_dev);
extern int ulog_logfile_close(int logfd);
extern size_t ulog_logfile_save(int logfd, const void *buf, size_t size);
extern int ulog_logfile_create_fullname(int file_type, const char *fullname, long tftp_size, int is_dump);
extern int tftp_write_request(const char *filename, long tsize);
extern int tftp_test_server(const char *serv_ip);
extern int ftp_write_request(int index, const char *ftp_server, const char *user, const char *pass, const char *filename);


