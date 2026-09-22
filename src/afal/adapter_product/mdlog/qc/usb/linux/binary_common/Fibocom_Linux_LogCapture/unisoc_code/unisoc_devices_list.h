#ifndef __UNISOC_DEVICES_LIST_H__
#define __UNISOC_DEVICES_LIST_H__

#include "misc_usb.h"

extern int unisoc_log_main(int argc, char** argv);

static fibo_usbdev_t unisoc_devices_table[] =
{
    //pcie
    {"FIBOCOM UNISOC UDX710 PCIE", 0x16c3, 0xabcd, {-1, -1},  unisoc_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    /*zhangboxing  2022/12/02 begin*/
    //710
    {"FIBOCOM UNISOC UDX710", 0x2CB7, 0x0a04, {3, 4},  unisoc_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM UNISOC UDX710", 0x2CB7, 0x0a05, {4, 5},  unisoc_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM UNISOC UDX710", 0x2CB7, 0x0a06, {4, 5},  unisoc_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM UNISOC UDX710", 0x2CB7, 0x0a07, {4, 5},  unisoc_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM UNISOC UDX710", 0x2CB7, 0x0a08, {4, 5},  unisoc_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    /*zhangboxing  2022/12/02 end*/

    //710 apdump
    {"FIBOCOM UNISOC UDX710", 0x2CB7, 0x0a09, {-1, -1},  unisoc_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM UNISOC UDX710", 0x1782, 0x4d00, {-1, -1},  unisoc_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},

};

#endif

