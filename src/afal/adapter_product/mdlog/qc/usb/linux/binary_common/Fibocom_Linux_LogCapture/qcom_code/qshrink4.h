#ifndef __QSHRINK4_H
#define __QSHRINK4_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>


#define MDM_RSP_SIZE (64*1024)      /* 2020-11-26 added to fix mantis 63061 */


#define DIAG_MODEM_PROC     0
#define DIAG_APPS_PROC      7

#define GUID_LEN            16
#define MAX_GUID_ENTRIES    16
#define NUM_PROC 10

/* Invalid Command Response */
#define DIAG_BAD_CMD_F  19

/* Invalid parmaeter Response */
#define DIAG_BAD_PARM_F 20

/* Invalid packet length Response */
#define DIAG_BAD_LEN_F  21

/* Subssytem dispatcher (extended diag cmd) */
#define DIAG_SUBSYS_CMD_F   75

/* Subssytem dispatcher Version 2 (delayed response capable) */
#define DIAG_SUBSYS_CMD_VER_2_F    128

enum {
  DIAG_SUBSYS_DIAG_SERV = 18,      /* DIAG Services */
};

#define DIAG_GET_DIAG_ID    0x222

#define DIAGDIAG_QSR4_FILE_OP_MODEM            0x0816

#define DIAGDIAG_FILE_LIST_OPERATION           0x00
#define DIAGDIAG_FILE_OPEN_OPERATION           0x01
#define DIAGDIAG_FILE_READ_OPERATION           0x02
#define DIAGDIAG_FILE_CLOSE_OPERATION          0x03

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} __attribute__ ((packed))  GUID;

typedef struct {
    uint8_t cmd_code;
    uint8_t subsys_id;
    uint16_t subsys_cmd_code;
} __attribute__ ((packed)) diag_pkt_header_t;

typedef struct  {
    uint8_t cmd_code;
    uint8_t subsys_id;
    uint16_t subsys_cmd_code;
    uint16_t version;
    uint16_t opcode;
}  __attribute__ ((packed)) diag_qsr_header_req;

typedef struct {
    uint8_t cmd_code;
    uint8_t subsys_id;
    uint16_t subsys_cmd_code;
    uint32_t delayed_rsp_status;
    uint16_t delayed_rsp_id;
    uint16_t rsp_cnt;
    uint16_t version;
    uint16_t opcode;
} __attribute__ ((packed)) diag_qsr_header_rsp;

typedef struct {
    uint8_t guid[16];
    uint32_t file_len;
} __attribute__ ((packed)) qshrink4_file_info;

typedef struct {
    diag_qsr_header_rsp rsp_header;
    uint8_t status;
    uint8_t num_files;
    qshrink4_file_info info[0];
} __attribute__ ((packed)) diag_qsr_file_list_rsp;

typedef struct  {
    diag_qsr_header_req req;
    uint8_t guid[16];
} __attribute__ ((packed)) diag_qsr_file_open_req;

typedef struct  {
    diag_qsr_header_rsp rsp_header;
    uint8_t guid[16];
    uint16_t read_file_fd;
    uint8_t status;
}  __attribute__ ((packed)) diag_qsr_file_open_rsp;

typedef struct {
    diag_qsr_header_req req;
    uint16_t read_file_fd;
} __attribute__ ((packed))  diag_qsr_file_close_req;

typedef struct  {
    diag_qsr_header_rsp rsp_header;
    uint16_t read_file_fd;
    uint8_t status;
} __attribute__ ((packed)) diag_qsr_file_close_rsp;

typedef struct {
    diag_qsr_header_req req;
    uint16_t read_file_fd;
    uint32_t req_bytes;
    uint32_t offset;
} __attribute__ ((packed)) diag_qsr_file_read_req;

typedef struct {
    uint8_t cmd_code;
    uint8_t subsys_id;
    uint16_t subsys_cmd_code;
    uint32_t delayed_rsp_status;
    uint16_t delayed_rsp_id;
    uint16_t rsp_cnt;
    uint16_t version;
    uint16_t opcode;
    uint16_t read_file_fd;
    uint32_t offset;
    uint32_t num_read;
    uint8_t status;
    uint8_t data[0];
}  __attribute__ ((packed)) diag_qsr_file_read_rsp;

typedef struct {
    uint32_t header_length;
    uint8_t version;
    uint8_t hdlc_data_type;
    uint32_t guid_list_entry_count;
    uint8_t guid[MAX_GUID_ENTRIES][GUID_LEN];
    uint32_t guid_file_len[MAX_GUID_ENTRIES];
} __attribute__ ((packed)) qshrink4_header;

typedef struct {
    uint8_t diag_id;
    char process_name[30];
    uint8_t guid[GUID_LEN];
}__attribute__ ((packed)) diagid_guid_struct;

typedef struct {
    uint8_t cmd_code;
    uint8_t subsys_id;
    uint16_t subsys_cmd_code;
    uint8_t version;
} __attribute__ ((packed)) diag_id_list_req;

typedef struct {
    uint8_t diag_id;
    uint8_t len;
    char process_name[];
} __attribute__ ((packed)) diag_id_entry_struct;

typedef struct {
    uint8_t cmd_code;
    uint8_t subsys_id;
    uint16_t subsys_cmd_code;
    uint8_t version;
    uint8_t num_entries;
    diag_id_entry_struct entry;
} __attribute__ ((packed)) diag_id_list_rsp;

typedef struct {
    uint8_t diag_id;
    uint8_t peripheral;
    char process_name[30];
} __attribute__ ((packed)) diag_id_list;

#define MAX_QSR4_DB_FILE_READ_PER_RSP   4000

typedef enum {
    DB_PARSER_STATE_OFF,
    DB_PARSER_STATE_ON,
    DB_PARSER_STATE_LIST,
    DB_PARSER_STATE_OPEN,
    DB_PARSER_STATE_READ,
    DB_PARSER_STATE_CLOSE,
} qsr4_db_file_parser_state;

extern qshrink4_header qshrink4_data;


void qcom_send_empty_mask(void);
void qcom_set_skip_empty_mask(int skip);
int qcom_qshrink4_header_ready(void);
int qcom_init_offline_qshrink4_header(void);
int qcom_init_qmdl2_header_from_modem(int fd, const char *log_dir);
int qcom_create_qshrink4_file(int fd, const char *log_dir);
void qcom_parse_data_for_command_rsp(const uint8_t *src_ptr, size_t src_length);


#endif
