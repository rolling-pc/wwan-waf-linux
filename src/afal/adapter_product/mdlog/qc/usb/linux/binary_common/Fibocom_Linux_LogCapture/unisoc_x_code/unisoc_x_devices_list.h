#ifndef __UNISOC_DEVICES_LIST_H__
#define __UNISOC_DEVICES_LIST_H__

#include "misc_usb.h"

//Export type definitions
#define DIAG_HDR_LEN     8

/* DIAG */
#define DIAG_SWVER_F             0                   // Information regarding MS software
#define DIAG_SYSTEM_F            5
#define MSG_BOARD_ASSERT         255

#define ITC_REQ_TYPE	         209
#define ITC_REQ_SUBTYPE          100
#define ITC_REP_SUBTYPE          101

#define UET_SUBTYPE_CORE0        102                 // UE Time Diag SubType field, core0
#define TRACET_SUBTYPE           103                 // Trace Time Diag subtype filed
#define UET_SUBTYPE_CORE1        104                 // UE Time Diag SubType field, core1

#define UET_SEQ_NUM              0x0000FFFF          // UE Time Diag Sequnce number fi
#define UET_DIAG_LEN             (DIAG_HDR_LEN + 12) // UE Time Diag Length field (20)

/* ASSERT */
#define NORMAL_INFO              0x00
#define DUMP_MEM_DATA            0x01
#define DUMP_ALL_ASSERT_INFO_END 0x04
#define DUMP_AP_MEMORY           0x08

typedef struct _diag_header
{
	unsigned int sn;			// Sequence number
    unsigned short len;			// Package length
	unsigned char type;
	unsigned char subtype;
} DIAG_HEADER;


typedef struct Protocol_Header_Tag
{
    uint32_t sequenceNum;
    uint16_t length;
    uint8_t type;
    uint8_t subtype;
} Protocol_Header;

typedef struct Protocol_TimeStamp_Tag
{
    Protocol_Header header;
    uint64_t llUETime;
    uint32_t dwUETickCount;
} Protocol_TimeStamp;

typedef struct _ch_timestamp{
    uint8_t sync;
    uint8_t lenM;
    uint8_t lenL;
    uint8_t flowid;
    uint32_t date;
    uint32_t ms;
} CH_TIMESTAMP;

extern int unisoc_x_log_main(int argc, char** argv);

static fibo_usbdev_t unisoc_x_devices_table[] =
{
    //8910
    //{"FIBOCOM UNISOC 8910", 0x1782, 0x4D10, {3, 4},  unisoc_x_log_main, -1, {-1, -1}},
    //{"FIBOCOM UNISOC 8910", 0x1782, 0x4D11, {3, 4},  unisoc_x_log_main, -1, {-1, -1}},
    {"FIBOCOM UNISOC 8910", 0x1782, 0x4D10, {4, 3},  unisoc_x_log_main, -1, {-1, -1}},
    {"FIBOCOM UNISOC 8910", 0x1782, 0x4D11, {6, 5},  unisoc_x_log_main, -1, {-1, -1}},

    //8310
    {"FIBOCOM UNISOC 8310", 0x1782, 0x4D22, {3, 4},  unisoc_x_log_main, -1, {-1, -1}},

    //8850
    {"FIBOCOM UNISOC 8850", 0x2CB7, 0x0A0A, {1, 2},  unisoc_x_log_main, -1, {-1, -1}},
    {"FIBOCOM UNISOC 8850", 0x2CB7, 0x0A0B, {3, 4},  unisoc_x_log_main, -1, {-1, -1}},
    {"FIBOCOM UNISOC 8850", 0x2CB7, 0x0A08, {3, 4},  unisoc_x_log_main, -1, {-1, -1}},
    {"FIBOCOM UNISOC 8850", 0x2CB7, 0x0A0C, {3, 4},  unisoc_x_log_main, -1, {-1, -1}},
    {"FIBOCOM UNISOC 8850", 0x1782, 0x4D17, {1, 3},  unisoc_x_log_main, -1, {-1, -1}},

};

#endif

