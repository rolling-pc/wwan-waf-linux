#ifndef __EIGENCOMM_DEVICES_LIST_H__
#define __EIGENCOMM_DEVICES_LIST_H__

#include "misc_usb.h"

extern int eigencomm_log_main(int argc,char *argv[]);


static fibo_usbdev_t eigencomm_devices_table[] =
{
    {"FIBOCOM LE", 0x2CB7, 0x0d01, {4, -1},  eigencomm_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM LE", 0x2CB7, 0x0111, {3, -1},  eigencomm_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
};


#endif

