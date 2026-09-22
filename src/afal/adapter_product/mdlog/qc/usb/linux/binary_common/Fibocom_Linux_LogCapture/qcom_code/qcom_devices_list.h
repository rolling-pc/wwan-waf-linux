#ifndef __QCOM_DEVICES_LIST_H__
#define __QCOM_DEVICES_LIST_H__

#include "qlog.h"

extern int qcom_log_main(int argc, char **argv);
extern int qcom_dumplog_main(int argc, char **argv);
extern fibo_usbdev_t *fibo_detect_fibocom_modules(char *portname);


static fibo_usbdev_t qcom_devices_table[] =
{
    //SDX12/X62/X55
    //dump and pcie diag index, do not change.
    {"FIBOCOM SDX12/X62/X55",         0x05C6, 0x900e, {0, -1},  qcom_dumplog_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM PCIE",                  0x0000, 0x0000, {-1, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX12/X62/X55/MDM9X07", 0x2CB7, 0x0110, {3, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX12/X62/X55",         0x2CB7, 0x0104, {0, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX12/X62/X55",         0x2CB7, 0x0105, {0, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX12/X62/X55",         0x2CB7, 0x010b, {3, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX12/X62/X55",         0x2CB7, 0x0111, {3, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX12/X62/X55",         0x2CB7, 0x01A2, {4, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX12/X62/X55",         0x2CB7, 0x01A4, {4, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX12/X62/X55",         0x33F8, 0x01A2, {4, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX12/X62/X55",         0x33F8, 0x01A4, {4, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX12/X62/X55",         0x05C6, 0x90DB, {0, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},

    //MDM9607
    {"FIBOCOM MDM9X07", 0x1508, 0x1001, {0, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM MDM9X07", 0x2CB7, 0x01A6, {2, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM MDM9X07", 0x2ca3, 0x4009, {0, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},

    //MDM9205
    {"FIBOCOM MDM9205", 0x2CB7, 0x0106, {0, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM MDM9205", 0x2CB7, 0x01A5, {0, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM MDM9205", 0x05C6, 0x90b2, {0, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},

    //QCOM
    {"FIBOCOM MDM9X07", 0x05C6, 0x9025, {0, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM MDM9X07", 0x05C6, 0x90B6, {3, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},

    //SDX35
    {"FIBOCOM SDX35",   0x2CB7, 0x0112, {1, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX35",   0x2CB7, 0x0113, {2, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX35",   0x2CB7, 0x0114, {2, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX35",   0x2CB7, 0x0115, {2, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX35",   0x33F8, 0x0115, {3, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    {"FIBOCOM SDX35",   0x33F8, 0x1003, {3, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},
    //AVM
    {"FIBOCOM SDX35",   0x2CB7, 0x0116, {3, -1},  qcom_log_main, -1, {-1, -1}, {0},{0},{0},{0},{0},0,0,NULL,NULL},

};


#endif

