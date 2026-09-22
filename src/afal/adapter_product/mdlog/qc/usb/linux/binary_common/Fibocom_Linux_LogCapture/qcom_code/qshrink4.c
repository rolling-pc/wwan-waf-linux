#include "qlog.h"

static int log_debug_en = 0;

static pthread_mutex_t qcom_cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t qcom_cmd_cond = PTHREAD_COND_INITIALIZER;

static int s_parser_state;
static int diag_id_state;

qshrink4_header qshrink4_data;

static uint8_t diag_id_count;
static diagid_guid_struct diagid_guid[MAX_GUID_ENTRIES];
static diag_id_list diag_id_table[NUM_PROC];

static uint8_t cur_peripheral = DIAG_MODEM_PROC;
static int dm_fd = -1;
static uint8_t qcom_rsp_buf[MDM_RSP_SIZE];
static size_t qcom_rsp_cur = 0;
static int read_file_fd = -1;
static uint32_t read_file_total_len = 0;
static uint32_t read_file_len = 0;
static int disk_file_fd = -1;

static int qlog_create_file_in_logdir(const char *log_dir, const char *filename)
{
    int fd = -1;
    char fullname[512] = {0};

    sprintf(fullname, "%s/%s", log_dir, filename);

    fd = open(fullname, O_CREAT | O_WRONLY | O_TRUNC, 0777);
    if (fd < 0) {
        printf("[%s] open %s failed. errno:%d(%s)\n", __func__, fullname, errno, strerror(errno));
    }

    return fd;
}

static void qcom_add_qshrink4_header(void)
{
    uint32_t header_length = 0;
    uint32_t *data_ptr = NULL;
//  int w_len = -1;
    uint32_t count = qlog_le32(qshrink4_data.guid_list_entry_count);

    header_length = 10 + count*GUID_LEN + sizeof(count) + count*sizeof(diagid_guid_struct);

    qshrink4_data.version = 2;
    qshrink4_data.hdlc_data_type = 1;

    data_ptr = (uint32_t *)(&qshrink4_data.guid[count][0]);
    *data_ptr++ = qshrink4_data.guid_list_entry_count;
    memcpy(data_ptr, diagid_guid, count*sizeof(diagid_guid_struct));

    qshrink4_data.header_length = qlog_le32(header_length);

    //fd = qlog_create_file_in_logdir("qlog.qmdl2");
//    if (fd != -1) {
//        w_len = write(fd, &qshrink4_data, header_length);
//        /* printf("[%s] write %d\n", __func__, w_len); */
//        if (w_len);
//        close(fd);
//    }
}

static uint8_t qsr4_db_cmd_req_buf[100];

#define CRC_16_L_SEED           0xFFFF
#define CRC_TAB_SIZE            256     /* 2^CRC_TAB_BITS */
#define CRC_16_L_POLYNOMIAL     0x8408

const unsigned short crc_16_l_table[ CRC_TAB_SIZE ] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static unsigned short crc_16_l_calc(const uint8_t *buf_ptr, int len)
{
    int data, crc_16;

    for (crc_16 = CRC_16_L_SEED; len >= 8; len -= 8, buf_ptr++) {
        crc_16 = crc_16_l_table[(crc_16 ^ *buf_ptr) & 0x00ff] ^ (crc_16 >> 8);
    }

    if (len != 0) {
        data = ((int) (*buf_ptr)) << (16 - 8);
        while (len-- != 0) {
            if (((crc_16 ^ data) & 0x01) != 0) {
                crc_16 >>= 1;
                crc_16 ^= CRC_16_L_POLYNOMIAL;
            } else {
                crc_16 >>= 1;
            }

            data >>= 1;
        }
    }

    return (~crc_16);
}

static int wait_for_response(int *p_wait_zero)
{
    struct timespec time;
    struct timeval now;
    int rt = 0;
    int i = 0;

    if (*p_wait_zero) {
        if (log_debug_en) printf("[%s] p_wait_zero: %d\n", __func__, *p_wait_zero);
    }

    for (i = 0; i < 10; i++) {
        if (*p_wait_zero == 0) {
            rt = 0;
            break;
        }

        gettimeofday(&now, NULL);
        time.tv_sec = now.tv_sec + 1;
        time.tv_nsec = now.tv_usec;

        pthread_mutex_lock(&qcom_cmd_mutex);
        rt = pthread_cond_timedwait(&qcom_cmd_cond, &qcom_cmd_mutex, &time);
        pthread_mutex_unlock(&qcom_cmd_mutex);

        if (rt != ETIMEDOUT)
            break;
    }

    *p_wait_zero = 0;

    return rt;
}

static void wakeup_response(int *p_wait_zero)
{
    if (log_debug_en) printf("[%s] p_wait_zero: %d\n", __func__, *p_wait_zero);
    *p_wait_zero = 0;
    pthread_mutex_lock(&qcom_cmd_mutex);
    pthread_cond_signal(&qcom_cmd_cond);
    pthread_mutex_unlock(&qcom_cmd_mutex);
}

static uint16_t diag_qsr4_append_crc16(uint8_t *buf_ptr, int len)
{
    uint16_t crc = crc_16_l_calc(buf_ptr, len*8);

    buf_ptr[len++] = crc&0xFF;
    buf_ptr[len++] = (crc>>8)&0xFF;
    buf_ptr[len++] = 0x7E;

    return len;
}

static const char *guid_file_name(const uint8_t *guid)
{
    static char read_buf[128];
    GUID *guid_val = (GUID *)guid;

    snprintf(read_buf, sizeof(read_buf), "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x.qdb", qlog_le32(guid_val->data1),
               qlog_le16(guid_val->data2), qlog_le16(guid_val->data3), guid_val->data4[0], guid_val->data4[1], guid_val->data4[2],
               guid_val->data4[3], guid_val->data4[4], guid_val->data4[5], guid_val->data4[6], guid_val->data4[7]);

    return read_buf;
}

static int diag_send_data(uint8_t *buf, size_t bytes)
{
    size_t i, size = 0;
    uint8_t *dst = &qsr4_db_cmd_req_buf[bytes];

    bytes -= 1; //skip 0x7E
    for (i = 0; i < bytes; i++) {
        uint8_t ch = buf[i];

        if (ch == 0x7E || ch == 0x7D) {
            dst[size++] = 0x7D;
            dst[size++] = (0x20 ^ ch);
        } else {
            dst[size++] = ch;
        }
    }
    dst[size++] = 0x7E;

    if (dm_fd != -1) {
        return qcom_send_cmd(dm_fd, dst, size);
    }

    return 0;
}

static int diag_qsr4_get_cmd_code_for_peripheral(int peripheral)
{
    switch (peripheral) {
        case DIAG_MODEM_PROC:
            return DIAGDIAG_QSR4_FILE_OP_MODEM;
        default:
            return -1;
    }
}

static int diag_query_pd_name(char *process_name, char *search_str)
{
    if (!process_name)
        return -EINVAL;

    if (strstr(process_name, search_str))
        return 1;

    return 0;
}

static int diag_query_pd(char *process_name)
{
    if (!process_name)
        return -EINVAL;

    if (diag_query_pd_name(process_name, "APPS"))
        return DIAG_APPS_PROC;
    if (diag_query_pd_name(process_name, "Apps"))
        return DIAG_APPS_PROC;
    if (diag_query_pd_name(process_name, "modem/root_pd"))
        return DIAG_MODEM_PROC;

    return -EINVAL;
}

static diag_id_list *get_diag_id(int peripheral)
{
    diag_id_list *item = NULL;
    uint8_t i;

    for (i = 0; i < diag_id_count; i++) {
        item = &diag_id_table[i];

        if ((peripheral == item->peripheral) && diag_query_pd_name(item->process_name, "root")) {
            return item;
        }
    }

    return NULL;
}

static void insert_diag_qsr4_db_guid_to_list(qshrink4_file_info* db_file_info, int peripheral)
{
    diag_id_list *item = NULL;
    qshrink4_header *qshrink4_data_ptr;
    uint32_t guid_list_entry_count;


    qshrink4_data_ptr = &qshrink4_data;
    guid_list_entry_count = qlog_le32(qshrink4_data_ptr->guid_list_entry_count);
    //printf("[%s] qshrink4_data_ptr->guid_list_entry_count: %d\n", __func__, qlog_le32(qshrink4_data_ptr->guid_list_entry_count));

    if (guid_list_entry_count >= MAX_GUID_ENTRIES)
        return;

    memcpy(&qshrink4_data_ptr->guid[guid_list_entry_count], db_file_info->guid, GUID_LEN);
    qshrink4_data_ptr->guid_file_len[guid_list_entry_count] = qlog_le32(db_file_info->file_len);

    item = get_diag_id(peripheral);
    if (item) {
        diagid_guid[guid_list_entry_count].diag_id = item->diag_id;
        strncpy(diagid_guid[guid_list_entry_count].process_name, item->process_name, 30);
    }
    memcpy(&diagid_guid[guid_list_entry_count].guid, db_file_info->guid, GUID_LEN);

    guid_list_entry_count++;
    qshrink4_data_ptr->guid_list_entry_count = qlog_le32(guid_list_entry_count);
    //printf("[%s] qshrink4_data_ptr->guid_list_entry_count:%d\n", __func__, qlog_le32(qshrink4_data_ptr->guid_list_entry_count));
}

static int diag_send_qsr4_db_file_list_cmd_req(int peripheral)
{
    uint8_t *ptr = qsr4_db_cmd_req_buf;
    diag_qsr_header_req *req = NULL;

    //printf("[%s] peripheral:%d\n", __func__, peripheral);

    if (diag_qsr4_get_cmd_code_for_peripheral(peripheral) == -1)
        return 0;

    cur_peripheral = peripheral;

    req = (diag_qsr_header_req*)ptr;

    req->cmd_code = DIAG_SUBSYS_CMD_VER_2_F;
    req->subsys_id = DIAG_SUBSYS_DIAG_SERV;
    req->subsys_cmd_code = qlog_le16(diag_qsr4_get_cmd_code_for_peripheral(peripheral));
    req->version = qlog_le16(1);
    req->opcode = qlog_le16(DIAGDIAG_FILE_LIST_OPERATION);

    s_parser_state = DB_PARSER_STATE_OPEN;
    diag_send_data(ptr, diag_qsr4_append_crc16(ptr, sizeof(diag_qsr_header_req)));
    wait_for_response(&s_parser_state);

    return 0;
}

static void process_qsr4_db_file_list_response(const uint8_t *buf, size_t size)
{
    diag_qsr_file_list_rsp *list_rsp = (diag_qsr_file_list_rsp *)buf;
    uint8_t i;

    if (list_rsp->status == 0) {
        for (i = 0; i < list_rsp->num_files; i++) {
            if (qlog_le32(list_rsp->info[i].file_len)) {
                //printf("[%s] guid len: %u, guid_file_name:%s\n", __func__, qlog_le32(list_rsp->info[i].file_len), guid_file_name(list_rsp->info[i].guid));
                insert_diag_qsr4_db_guid_to_list(&list_rsp->info[i], cur_peripheral);
            } else {
                if(list_rsp->num_files == 1) { /*MTC0732-1790*/
                    printf("qsr4 %s length=0,add guid\n",guid_file_name(list_rsp->info[i].guid));
                    insert_diag_qsr4_db_guid_to_list(&list_rsp->info[i], cur_peripheral);
                }
            }
        }
    }
    else {
        printf("[%s] status %d, size %zu\n", __func__, list_rsp->status, size);
    }

    wakeup_response(&s_parser_state);
}

static int diag_send_qsr4_file_open_cmd_req(const char *log_dir, int idx)
{
    uint8_t* ptr = qsr4_db_cmd_req_buf;
    diag_qsr_file_open_req* req_ptr;

    //printf("[%s] idx:%d\n", __func__, idx);

    req_ptr = (diag_qsr_file_open_req*)ptr;
    req_ptr->req.cmd_code = DIAG_SUBSYS_CMD_VER_2_F;
    req_ptr->req.subsys_id = DIAG_SUBSYS_DIAG_SERV;
    req_ptr->req.subsys_cmd_code = qlog_le16(diag_qsr4_get_cmd_code_for_peripheral(cur_peripheral));
    req_ptr->req.version = qlog_le16(1);
    req_ptr->req.opcode = qlog_le16(DIAGDIAG_FILE_OPEN_OPERATION);
    memcpy(req_ptr->guid, qshrink4_data.guid[idx], GUID_LEN);

    read_file_total_len = 0;
    read_file_fd = -1;
    s_parser_state = DB_PARSER_STATE_OPEN;
    diag_send_data(qsr4_db_cmd_req_buf, diag_qsr4_append_crc16(ptr, sizeof(diag_qsr_file_open_req)));
    wait_for_response(&s_parser_state);

    if (read_file_fd != -1) {
        read_file_total_len = qlog_le32(qshrink4_data.guid_file_len[idx]);
        disk_file_fd = qlog_create_file_in_logdir(log_dir, guid_file_name(qshrink4_data.guid[idx]));
    }

    return 0;
}

static void process_qsr_db_file_open_rsp(const uint8_t *buf, size_t size)
{
    diag_qsr_file_open_rsp *open_rsp = (diag_qsr_file_open_rsp*)buf;

    if (open_rsp->status == 0)
    {
        //printf("[%s] open read_file_fd: %d\n", __func__, qlog_le16(open_rsp->read_file_fd));
        read_file_fd = qlog_le16(open_rsp->read_file_fd);
    }
    else {
        printf("[%s] status: %d, size %zu\n", __func__, open_rsp->status, size);
    }

    wakeup_response(&s_parser_state);
}

static int diag_send_qsr4_file_close_send_req(int idx)
{
    uint8_t* ptr = qsr4_db_cmd_req_buf;
    diag_qsr_file_close_req* req_ptr;

    printf("[%s] idx:%d\n", __func__, idx);

    if (read_file_fd == -1) {
        return 0;
    }

    if (disk_file_fd != -1) {
        close(disk_file_fd);
        disk_file_fd = -1;
    }

    req_ptr = (diag_qsr_file_close_req*)ptr;
    req_ptr->req.cmd_code = DIAG_SUBSYS_CMD_VER_2_F;
    req_ptr->req.subsys_id = DIAG_SUBSYS_DIAG_SERV;
    req_ptr->req.subsys_cmd_code = qlog_le16(DIAGDIAG_QSR4_FILE_OP_MODEM);
    req_ptr->req.version = qlog_le16(1);
    req_ptr->req.opcode = qlog_le16(DIAGDIAG_FILE_CLOSE_OPERATION);
    req_ptr->read_file_fd = qlog_le16(read_file_fd);

    s_parser_state = DB_PARSER_STATE_CLOSE;
    diag_send_data(ptr, diag_qsr4_append_crc16(ptr, sizeof(diag_qsr_file_close_req)));
    wait_for_response(&s_parser_state);
    read_file_fd = -1;

    return 0;
}

static void process_qsr_db_file_close_rsp(const uint8_t *buf, size_t size)
{
    diag_qsr_file_close_rsp *close_rsp = (diag_qsr_file_close_rsp*)buf;

    if (qlog_le32(close_rsp->status) == 0)
    {
        //printf("[%s] close read_file_fd %d\n", __func__, qlog_le16(close_rsp->read_file_fd));
        if (read_file_fd == qlog_le16(close_rsp->read_file_fd)) {
            read_file_fd = -1;
        }
    }
    else {
        printf("[%s] status %d, size %zu\n", __func__, qlog_le16(close_rsp->status), size);
    }

    wakeup_response(&s_parser_state);
}

static int diag_send_qsr4_file_read_cmd_req(unsigned int file_offset, int file_len)
{
    uint8_t* ptr = qsr4_db_cmd_req_buf;
    diag_qsr_file_read_req* req;

    if (read_file_fd == -1)
        return 0;

    if (file_offset >= read_file_total_len)
        return 0;

    if ((file_offset + file_len) > read_file_total_len) {
        file_len = read_file_total_len - file_offset;
    }

    if (log_debug_en || (file_offset == 0 || ((file_offset + file_len) == read_file_total_len)))
        printf("[%s] offset:%d, len:%d\n", __func__, file_offset, file_len);

    req = (diag_qsr_file_read_req*)ptr;
    req->req.cmd_code = DIAG_SUBSYS_CMD_VER_2_F;
    req->req.subsys_id = DIAG_SUBSYS_DIAG_SERV;
    req->req.subsys_cmd_code = qlog_le16(diag_qsr4_get_cmd_code_for_peripheral(cur_peripheral));
    req->req.version = qlog_le16(1);
    req->req.opcode = qlog_le16(DIAGDIAG_FILE_READ_OPERATION);
    req->read_file_fd = qlog_le16(read_file_fd);
    req->offset = qlog_le32(file_offset);
    req->req_bytes = qlog_le32(file_len);

    read_file_len = 0;
    s_parser_state = DB_PARSER_STATE_READ;
    diag_send_data(ptr, diag_qsr4_append_crc16(ptr, sizeof(diag_qsr_file_read_req)));
    wait_for_response(&s_parser_state);

    return read_file_len;
}

static void process_qsr_db_file_read_delayed_rsp(const uint8_t *buf, size_t size)
{
    //int w_len = -1;
    diag_qsr_file_read_rsp *read_rsp = (diag_qsr_file_read_rsp *)buf;

    if (read_rsp->status == 0) {
        uint32_t num_read = qlog_le32(read_rsp->num_read);
        if (log_debug_en) printf("[%s] offset: %d, len:%d, rsp_cnt:%d\n", __func__, qlog_le32(read_rsp->offset), num_read , qlog_le16(read_rsp->rsp_cnt));
        if (num_read) {
            if (disk_file_fd != -1) {
                (void)write(disk_file_fd, read_rsp->data, num_read);
                /*2020.11.17 zhangboxing masking invalid log */
                /* printf("[%s] write %d\n", __func__, w_len); */

            }
            read_file_len = num_read;
            wakeup_response(&s_parser_state);
        }
    }
    else {
        printf("[%s] status: %d, size %zu\n", __func__, read_rsp->status, size);
        wakeup_response(&s_parser_state);
    }
}

static void insert_diag_id_entry(diag_id_entry_struct *entry)
{
    diag_id_list *new_entry;

    if (diag_id_count >= NUM_PROC)
        return;

    new_entry = &(diag_id_table[diag_id_count++]);

    new_entry->diag_id = entry->diag_id;
    strncpy(new_entry->process_name, entry->process_name, 30);
    new_entry->peripheral = diag_query_pd(new_entry->process_name);

    //printf("[%s] diag_id:%d, peripheral:%d, process_name:%s\n", __func__,new_entry->diag_id, new_entry->peripheral, new_entry->process_name);
}

static int diag_query_diag_id(void)
{
    uint8_t *ptr = qsr4_db_cmd_req_buf;
    diag_id_list_req* req = NULL;

    //printf("[%s]\n", __func__);

    req = (diag_id_list_req*)ptr;

    req->cmd_code = DIAG_SUBSYS_CMD_F;
    req->subsys_id = DIAG_SUBSYS_DIAG_SERV;
    req->subsys_cmd_code = qlog_le16(DIAG_GET_DIAG_ID);
    req->version = 1;

    diag_id_state = 1;
    diag_send_data(ptr, diag_qsr4_append_crc16(ptr, sizeof(diag_id_list_req)));
    wait_for_response(&diag_id_state);

    return 0;
}

static void process_diag_id_response(const uint8_t *buf, size_t size)
{
    diag_id_list_rsp *list_rsp = (diag_id_list_rsp*)buf;
    diag_id_entry_struct *diag_id_ptr;
    uint32_t i = 0;

    if (list_rsp->cmd_code != DIAG_SUBSYS_CMD_F || list_rsp->version != 1) {
        printf("[%s]: %d, size %zu\n", __func__,  __LINE__, size);
        wakeup_response(&diag_id_state);
        return;
    }

    diag_id_ptr = &list_rsp->entry;
    for (i = 0; i < list_rsp->num_entries; i++) {
        insert_diag_id_entry(diag_id_ptr);
        diag_id_ptr = (diag_id_entry_struct *)(((uint8_t *)diag_id_ptr) + diag_id_ptr->len + sizeof(diag_id_entry_struct));
    }

    wakeup_response(&diag_id_state);
}

static void qcom_handle_data_for_command_rsp(const uint8_t *buf, size_t size)
{
    if (diag_id_state)
    {
        diag_id_list_rsp *rsp = (diag_id_list_rsp*)buf;

        if (log_debug_en) printf("[%s] rx: %zd, %02x%02x%02x%02x %02x%02x%02x%02x\n", __func__, size, 
                buf[0],buf[1],buf[2],buf[3],buf[4], buf[5], buf[6], buf[7]);

        if ((buf[0] == DIAG_BAD_CMD_F || buf[0] == DIAG_BAD_PARM_F || buf[0] == DIAG_BAD_LEN_F)
            && buf[1] == DIAG_SUBSYS_CMD_F && buf[2] == DIAG_SUBSYS_DIAG_SERV)
        {
            printf("[%s] buf[0]:%d\n", __func__, buf[0]);
            return wakeup_response(&s_parser_state);
        }

        if (rsp->cmd_code == DIAG_SUBSYS_CMD_F && rsp->subsys_id == DIAG_SUBSYS_DIAG_SERV
            && qlog_le16(rsp->subsys_cmd_code) == DIAG_GET_DIAG_ID)
        {
            return process_diag_id_response(buf, size);
        }
    }
    else if (s_parser_state)
    {
        diag_qsr_header_rsp *rsp = (diag_qsr_header_rsp*)buf;

        if (log_debug_en) printf("rx: %zd, %02x%02x%02x%02x %02x%02x%02x%02x\n", size, buf[0],buf[1],buf[2],buf[3],buf[4], buf[5], buf[6], buf[7]);

        if ((buf[0] == DIAG_BAD_CMD_F || buf[0] == DIAG_BAD_PARM_F || buf[0] == DIAG_BAD_LEN_F)
            && buf[1] == DIAG_SUBSYS_CMD_VER_2_F && buf[2] == DIAG_SUBSYS_DIAG_SERV) {
            printf("[%s] buf[0]:%d\n", __func__, buf[0]);
            return wakeup_response(&s_parser_state);
        }

        if (rsp->cmd_code == DIAG_SUBSYS_CMD_VER_2_F && rsp->subsys_id == DIAG_SUBSYS_DIAG_SERV
            && qlog_le16(rsp->subsys_cmd_code) == DIAGDIAG_QSR4_FILE_OP_MODEM)
        {
            uint16_t opcode = qlog_le16(rsp->opcode);

            if (qlog_le16(rsp->version) != 1) {
                printf("[%s] version %d\n", __func__, qlog_le16(rsp->version));
                return wakeup_response(&s_parser_state);
            }
            else if (opcode == DIAGDIAG_FILE_LIST_OPERATION) {
                return process_qsr4_db_file_list_response(buf, size);
            }
            else if (opcode == DIAGDIAG_FILE_OPEN_OPERATION) {
                return process_qsr_db_file_open_rsp(buf, size);
            }
            else if (opcode == DIAGDIAG_FILE_CLOSE_OPERATION) {
                return process_qsr_db_file_close_rsp(buf, size);
            }
            else if (opcode == DIAGDIAG_FILE_READ_OPERATION) {
                return process_qsr_db_file_read_delayed_rsp(buf, size);
            }
            else {
                printf("[%s] opcode: 0x%04X\n", __func__, opcode);
            }
        }
    }
}

void qcom_parse_data_for_command_rsp(const uint8_t *src_ptr, size_t src_length)
{
    size_t i, len = 0;
    uint8_t *dest_ptr = qcom_rsp_buf;
    uint8_t src_byte = 0;

    //printf("[%s], line: %d\n", __func__, __LINE__);

_parse_pkt:
    len = qcom_rsp_cur;

    for (i = 0; i < src_length; i++)
    {
        src_byte = src_ptr[i];

        if (src_byte == 0x7D)
        {
            if (i == (src_length - 1))
            {
                i++;
                break;
            }
            else
            {
                dest_ptr[len++] = src_ptr[++i] ^ 0x20;
            }
        }
        else if (src_byte == 0x7E)
        {
            if (i == 0 && src_length > 1)
                continue;
            dest_ptr[len++] = src_byte;
            i++;
            break;
        }
        else
        {
            dest_ptr[len++] = src_byte;
        }

        if (len >= MDM_RSP_SIZE)
        {
            i++;
            break;
        }
    }

    if (len && dest_ptr[len-1] == 0x7E) {
        pthread_mutex_lock(&mutex); /*resolve the bug g_mdm_req issue (mantis 63061), yanghaitao 2020.11.25 */
        if (g_qcom_req && (g_qcom_req[0] == dest_ptr[0] || dest_ptr[0] == 0x13)) {
            g_qcom_req = NULL;
            //printf("%s.\n", __func__);
        }
        pthread_mutex_unlock(&mutex); /*resolve the bug g_mdm_req issue (mantis 63061), yanghaitao 2020.11.25 */
        qcom_handle_data_for_command_rsp(dest_ptr, len);
        qcom_rsp_cur = 0;

        //printf("[%s] len:%d\n", __func__, len);
    }
    else if (len > MDM_RSP_SIZE) {
        qcom_rsp_cur = 0;
        printf("[%s] too long mdm rsp len: %lu\n", __func__, len);
    }
    else {
        qcom_rsp_cur = len;
    }

    src_length -= i;
    src_ptr += i;
    if (src_length) {
        goto _parse_pkt;
    }
}

static char cmd_disable_log_mask[] = { 0x73, 0, 0, 0, 0, 0, 0, 0 };
static char cmd_disable_msg_mask[] = { 0x7D, 0x05, 0, 0, 0, 0, 0, 0 };
static char cmd_disable_event_mask[] = { 0x60, 0 };
static char cmd_disable_qtrace_mask[] = { 0x4B, 0x44, 0x01, 0x90, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

void qcom_send_empty_mask(void)
{
    uint8_t *ptr = qsr4_db_cmd_req_buf;

    memcpy(ptr, cmd_disable_log_mask, sizeof(cmd_disable_log_mask));
    diag_send_data(ptr, diag_qsr4_append_crc16(ptr, sizeof(cmd_disable_log_mask)));

    memcpy(ptr, cmd_disable_msg_mask, sizeof(cmd_disable_msg_mask));
    diag_send_data(ptr, diag_qsr4_append_crc16(ptr, sizeof(cmd_disable_msg_mask)));

    memcpy(ptr, cmd_disable_event_mask, sizeof(cmd_disable_event_mask));
    diag_send_data(ptr, diag_qsr4_append_crc16(ptr, sizeof(cmd_disable_event_mask)));

    memcpy(ptr, cmd_disable_qtrace_mask, sizeof(cmd_disable_qtrace_mask));
    diag_send_data(ptr, diag_qsr4_append_crc16(ptr, sizeof(cmd_disable_qtrace_mask)));
}

static int g_skip_empty_mask = 0;

void qcom_set_skip_empty_mask(int skip)
{
    g_skip_empty_mask = skip ? 1 : 0;
}

int qcom_qshrink4_header_ready(void)
{
    return qmdl2_v2_mode ? (qlog_le32(qshrink4_data.header_length) != 0) : 1;
}

int qcom_init_offline_qshrink4_header(void)
{
    memset(&qshrink4_data, 0, sizeof(qshrink4_data));
    qcom_add_qshrink4_header();
    return qcom_qshrink4_header_ready();
}

static size_t qlog_get_filesize_in_logdir(const char *log_dir, const char *filename)
{
    int fd = -1;
    size_t file_len = 0;
    char fullname[512] = {0};

    sprintf(fullname, "%s/%s", log_dir, filename);

    fd = open(fullname, O_RDONLY);
    if (fd >= 0) {
        file_len = lseek(fd, 0, SEEK_END);
        close(fd);
    }

    return file_len;
}

static void qshrink4_sync_guid_file_len_from_disk(const char *log_dir)
{
    uint32_t guid_count = qlog_le32(qshrink4_data.guid_list_entry_count);
    uint32_t i;

    for (i = 0; i < guid_count; i++) {
        size_t on_disk = qlog_get_filesize_in_logdir(log_dir, guid_file_name(qshrink4_data.guid[i]));
        uint32_t hdr_len = qlog_le32(qshrink4_data.guid_file_len[i]);

        if (on_disk == 0)
            continue;
        if (hdr_len != (uint32_t)on_disk) {
            printf("[%s] sync %s header len %u -> %zu\n",
                    __func__, guid_file_name(qshrink4_data.guid[i]), hdr_len, on_disk);
            qshrink4_data.guid_file_len[i] = qlog_le32((uint32_t)on_disk);
        }
    }
}

static void insert_diagid_placeholder(const diag_id_list *item)
{
    uint32_t count;
    uint32_t i;

    if (!item)
        return;

    count = qlog_le32(qshrink4_data.guid_list_entry_count);
    for (i = 0; i < count; i++) {
        if (diagid_guid[i].diag_id == item->diag_id)
            return;
    }

    if (count >= MAX_GUID_ENTRIES)
        return;

    memset(&qshrink4_data.guid[count], 0, GUID_LEN);
    qshrink4_data.guid_file_len[count] = 0;
    diagid_guid[count].diag_id = item->diag_id;
    strncpy(diagid_guid[count].process_name, item->process_name, 29);
    diagid_guid[count].process_name[29] = '\0';
    memset(diagid_guid[count].guid, 0, GUID_LEN);
    qshrink4_data.guid_list_entry_count = qlog_le32(count + 1);
}

int qcom_init_qmdl2_header_from_modem(int fd, const char *log_dir)
{
    uint8_t i;
    uint8_t listed[NUM_PROC];
    uint32_t gc;

    dm_fd = fd;
    memset(&qshrink4_data, 0, sizeof(qshrink4_data));
    diag_id_count = 0;
    memset(diagid_guid, 0, sizeof(diagid_guid));
    memset(diag_id_table, 0, sizeof(diag_id_table));
    s_parser_state = DB_PARSER_STATE_OFF;
    diag_id_state = 0;
    qcom_rsp_cur = 0;

    printf("[%s] query diag_id/GUID for qmdl2 header (no .qdb download)\n", __func__);
    diag_query_diag_id();

    printf("[%s] diag_id_count=%u\n", __func__, diag_id_count);
    memset(listed, 0, sizeof(listed));
    for (i = 0; i < diag_id_count; i++) {
        uint8_t periph = diag_id_table[i].peripheral;

        printf("[%s] pd[%u] diag_id=%u peripheral=%u name=%s\n",
                __func__, i, diag_id_table[i].diag_id, periph,
                diag_id_table[i].process_name);
        if (periph >= NUM_PROC || listed[periph])
            continue;
        listed[periph] = 1;
        diag_send_qsr4_db_file_list_cmd_req(periph);
    }

    if (qlog_le32(qshrink4_data.guid_list_entry_count) == 0) {
        for (i = 0; i < diag_id_count; i++)
            insert_diagid_placeholder(&diag_id_table[i]);
    }

    if (log_dir && log_dir[0])
        qshrink4_sync_guid_file_len_from_disk(log_dir);

    qcom_add_qshrink4_header();

    gc = qlog_le32(qshrink4_data.guid_list_entry_count);
    printf("[%s] header=%u bytes, GUID/diag_id entries=%u\n",
            __func__, qlog_le32(qshrink4_data.header_length), gc);
    return qcom_qshrink4_header_ready();
}

int qcom_create_qshrink4_file(int fd, const char *log_dir)
{
    static int qshrink4_init = 0;
    uint32_t guid_count;
    uint32_t i;

    if (qshrink4_init) {
        return qcom_qshrink4_header_ready();
    }
    qshrink4_init = 1;

    if (!qmdl2_v2_mode || diag_id_count != 0) {
        return qcom_qshrink4_header_ready();
    }

    dm_fd = fd;
    printf("[%s] fetching QSR4 dictionary...\n", __func__);

    if (!g_skip_empty_mask) {
        qcom_send_empty_mask();
    } else {
        printf("[%s] skip empty_mask (DMC masks already applied)\n", __func__);
    }

    diag_query_diag_id();

    for (i = 0; i < diag_id_count; i++) {
        diag_send_qsr4_db_file_list_cmd_req(diag_id_table[i].peripheral);
    }

    qshrink4_sync_guid_file_len_from_disk(log_dir);

    guid_count = qlog_le32(qshrink4_data.guid_list_entry_count);
    for (i = 0; i < guid_count; i++) {
        int read_len = 0;
        unsigned int total_len = 0;
        size_t guid_file_len = qlog_get_filesize_in_logdir(log_dir, guid_file_name(qshrink4_data.guid[i]));

        if (guid_file_len > 0 && guid_file_len == qlog_le32(qshrink4_data.guid_file_len[i])) {
            printf("[%s] reuse cached %s (%zu bytes)\n", __func__,
                    guid_file_name(qshrink4_data.guid[i]), guid_file_len);
            continue;
        }

        diag_send_qsr4_file_open_cmd_req(log_dir, i);
        do {
            read_len = diag_send_qsr4_file_read_cmd_req(total_len, MAX_QSR4_DB_FILE_READ_PER_RSP);
            total_len += read_len;
        } while (read_len == MAX_QSR4_DB_FILE_READ_PER_RSP);

        if (total_len > 0) {
            qshrink4_data.guid_file_len[i] = qlog_le32(total_len);
        }

        printf("[%s] downloaded %s (%u bytes)\n", __func__,
                guid_file_name(qshrink4_data.guid[i]), total_len);
        diag_send_qsr4_file_close_send_req(i);
    }

    qshrink4_sync_guid_file_len_from_disk(log_dir);
    qcom_add_qshrink4_header();

    if (qcom_qshrink4_header_ready()) {
        for (i = 0; i < guid_count; i++) {
            if (qlog_le32(qshrink4_data.guid_file_len[i]) == 0) {
                printf("[%s] WARNING %s missing/empty; B821 OTA may show encrypted in QXDM\n",
                        __func__, guid_file_name(qshrink4_data.guid[i]));
            }
        }
        printf("[%s] dictionary ready, header=%u bytes\n", __func__,
                qlog_le32(qshrink4_data.header_length));
        return 1;
    }

    printf("[%s] dictionary fetch finished but header is empty\n", __func__);
    return 0;
}
