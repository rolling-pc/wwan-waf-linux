#ifndef __ZTE_DEVICES_LIST_H__
#define __ZTE_DEVICES_LIST_H__

#include "misc_usb.h"

extern int zte_log_main(int argc, char** argv);


static fibo_usbdev_t zte_devices_table[] =
{
    {"FIBOCOM V3T/E", 0x2CB7, 0x0001, {5, -1},  zte_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM V3T/E", 0x19D2, 0x0579, {4, -1},  zte_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
};


#endif

