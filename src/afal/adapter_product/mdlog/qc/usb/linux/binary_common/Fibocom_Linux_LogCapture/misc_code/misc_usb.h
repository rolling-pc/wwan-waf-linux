#ifndef __MISC_USB_H__
#define __MISC_USB_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/usbdevice_fs.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 20)
#include <linux/usb/ch9.h>
#else
#include <linux/usb_ch9.h>
#endif

#include "misc.h"

#define USB_DIR_BASE    "/sys/bus/usb/devices"

#define MAX_USBFS_BULK_IN_SIZE      (16 * 1024)
#define MAX_USBFS_BULK_OUT_SIZE     (16 * 1024)

// #define MIN(X, Y)       ((X)<(Y))? (X):(Y)
#define EXTEND 128

#define FIBO_BUF_SIZE   512

typedef struct
{
    char *ModuleName;
    int idVendor;
    int idProduct;
    int ifnum[2];
    int (* log_main_function)(int argc, char** argv);
    int usbdev;
    int ttyfd[2];

    char portname[FIBO_BUF_SIZE];
    char syspath[FIBO_BUF_SIZE+EXTEND];
    char busname[FIBO_BUF_SIZE];

    int bulk_ep_in[2];
    int bulk_ep_out[2];
    int wMaxPacketSize;
    int usb_need_zero_package;

    int (* write)(const void *handle, void *pbuf, int size, int portnum);
    int (* read)(const void *handle, void *pbuf, int size, int portnum);
} fibo_usbdev_t;

fibo_usbdev_t *fibo_get_fibocom_device(fibo_usbdev_t *(* find_devices_in_table)(int idvendor, int idproduct), char *portname, int portnum);
int fibo_usb_open(fibo_usbdev_t *pdev, int portnum, int is_ttyUSB_first);
int fibo_usb_close(fibo_usbdev_t *pdev, int portnum);
int fibo_get_usb_Interface6_SubClass(fibo_usbdev_t *pdev);
int fibo_get_ttyport_by_syspath(fibo_usbdev_t *udev);
int strStartsWith(const char *line, const char *src);

#endif

