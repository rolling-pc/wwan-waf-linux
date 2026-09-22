#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include "glib-unix.h"
#include "poll.h"
#include "mbim_linux_helper_adapter.h"
#include "mbim_linux_helper_basic_func.h"
#include "libmbim_common_struct.h"
#include "log.hpp"
#include "common.hpp"

using namespace afal::log;
using namespace afal::error;

int thread_running = -1;
static int g_datatype = 0;
int recovery_flag = 0;

volatile sig_atomic_t g_msg_loop_ready = 0;
static pthread_cond_t  g_loop_cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_loop_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 两个消息队列ID */
static int g_req_queue_id = -1;   // 请求队列（主→辅）
static int g_resp_queue_id = -1;  // 响应队列（辅→主）

/* ---------- 辅助函数 ---------- */
static int rolling_adapter_get_queue_id(int type) {
    // type: 0 -> 请求队列, 1 -> 响应队列
    if (type == 0) {
        if (g_req_queue_id == -1) {
            key_t key = ftok(".", 0);
            if (key == ERR) {
                LOG_ERROR("ftok failed for req queue\n");
                return ERR;
            }
            g_req_queue_id = msgget(key, 0);
            if (g_req_queue_id == ERR) {
                LOG_ERROR("msgget req queue failed: %s\n", strerror(errno));
                return ERR;
            }
            LOG_DEBUG("req queue id: %d\n", g_req_queue_id);
        }
        return g_req_queue_id;
    } else {
        if (g_resp_queue_id == -1) {
            key_t key = ftok(".", 1);
            if (key == ERR) {
                LOG_ERROR("ftok failed for resp queue\n");
                return ERR;
            }
            g_resp_queue_id = msgget(key, 0);
            if (g_resp_queue_id == ERR) {
                LOG_ERROR("msgget resp queue failed: %s\n", strerror(errno));
                return ERR;
            }
            LOG_DEBUG("resp queue id: %d\n", g_resp_queue_id);
        }
        return g_resp_queue_id;
    }
}

static int rolling_deinit_message_queues(void) {
    int ret = OK;
    if (g_req_queue_id != -1) {
        if (msgctl(g_req_queue_id, IPC_RMID, NULL) == 0)
            LOG_DEBUG("req queue removed\n");
        else
            LOG_ERROR("req queue remove failed: %s\n", strerror(errno));
        g_req_queue_id = -1;
    }
    if (g_resp_queue_id != -1) {
        if (msgctl(g_resp_queue_id, IPC_RMID, NULL) == 0)
            LOG_DEBUG("resp queue removed\n");
        else
            LOG_ERROR("resp queue remove failed: %s\n", strerror(errno));
        g_resp_queue_id = -1;
    }
    return ret;
}

int clear_queue(int qid) {
    int ret;
    void *buf = malloc(8192);  // 足够大的临时缓冲区
    while ((ret = msgrcv(qid, buf, 8192, 0, IPC_NOWAIT)) != -1) {
        // 清空消息
    }
    free(buf);
    return (errno == ENOMSG) ? OK : ERR;
}

static int rolling_adapter_helper_queue_init(void) {
    // 创建请求队列（key=0）
    key_t key_req = ftok(".", 0);
    if (key_req == ERR) {
        LOG_ERROR("ftok for req failed\n");
        return ERR;
    }
    g_req_queue_id = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (g_req_queue_id == ERR) {
        LOG_ERROR("msgget req queue failed: %s\n", strerror(errno));
        return ERR;
    }
    LOG_DEBUG("req queue created, id=%d\n", g_req_queue_id);

    clear_queue(g_req_queue_id);

    // 创建响应队列（key=1）
    key_t key_resp = ftok(".", 1);
    if (key_resp == ERR) {
        LOG_ERROR("ftok for resp failed\n");
        return ERR;
    }
    g_resp_queue_id = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (g_resp_queue_id == ERR) {
        LOG_ERROR("msgget resp queue failed: %s\n", strerror(errno));
        return ERR;
    }
    LOG_DEBUG("resp queue created, id=%d\n", g_resp_queue_id);

    clear_queue(g_resp_queue_id);

    return OK;
}

/* ---------- 消息收发函数 ---------- */
// 发送请求（主→辅）
int rolling_adapter_helperd_send_req_to_helperm(void *msgs, gint msgsize) {
    (void)msgsize;
    int qid = rolling_adapter_get_queue_id(0);
    if (qid == ERR) {
        LOG_ERROR("Failed to get req queue id\n");
        return ERR;
    }
    int ret = msgsnd(qid, (void *)msgs, sizeof(void*), 0);
    if (ret == ERR) {
        LOG_ERROR("msgsnd to req queue failed: %s (errno=%d)\n", strerror(errno), errno);
        return ERR;
    }
    LOG_DEBUG("msgsnd req success, mtype=%ld\n", ((helper_message_struct*)msgs)->mtype);
    return OK;
}

// 接收请求（辅线程）
int rolling_adapter_helperm_get_normal_msg_from_helperd(void *msgs) {
    int qid = rolling_adapter_get_queue_id(0);
    if (qid == ERR) {
        LOG_ERROR("Failed to get req queue id\n");
        return ERR;
    }
    int ret = msgrcv(qid, (void *)msgs, sizeof(void*), MSG_NORMAL, IPC_NOWAIT);
    if (ret == ERR) {
        if (errno != ENOMSG && errno != EINTR) {
            LOG_ERROR("msgrcv req error: %s (errno=%d)\n", strerror(errno), errno);
        }
        return ERR;
    }
    LOG_DEBUG("msgrcv req success, mtype=%ld\n", ((helper_message_struct*)msgs)->mtype);
    return OK;
}

// 发送响应（辅→主）
int rolling_adapter_helperm_send_msg_to_helperd(void *msgs, gint msgsize) {
    (void)msgsize;
    int qid = rolling_adapter_get_queue_id(1);
    if (qid == ERR) {
        LOG_ERROR("Failed to get resp queue id\n");
        return ERR;
    }
    int ret = msgsnd(qid, (void *)msgs, sizeof(void*), 0);
    if (ret == ERR) {
        LOG_ERROR("msgsnd to resp queue failed: %s (errno=%d)\n", strerror(errno), errno);
        return ERR;
    }
    LOG_DEBUG("msgsnd resp success, mtype=%ld\n", ((helper_message_struct*)msgs)->mtype);
    return OK;
}

// 接收响应（主线程阻塞）
int rolling_adapter_helperd_get_normal_msg_from_helperm(void *msgs) {
    int qid = rolling_adapter_get_queue_id(1);
    if (qid == ERR) {
        LOG_ERROR("Failed to get resp queue id\n");
        return ERR;
    }
    int ret = msgrcv(qid, (void *)msgs, sizeof(void*), MSG_NORMAL, 0);
    if (ret == ERR) {
        LOG_ERROR("msgrcv resp error: %s (errno=%d)\n", strerror(errno), errno);
        if (ret == ERR && errno == E2BIG) {
            void *bigbuf = malloc(8192);
            if (bigbuf) {
                int len = msgrcv(qid, bigbuf, 8192, MSG_NORMAL, IPC_NOWAIT);
                if (len != -1) {
                    LOG_ERROR("E2BIG message length: %d\n", len);
                    // 打印前几个字节帮助定位
                }
                free(bigbuf);
            }
        }
        return ERR;
    }
    LOG_DEBUG("msgrcv resp success, mtype=%ld\n", ((helper_message_struct*)msgs)->mtype);
    return OK;
}

// 以下不常用，保留简单实现
int rolling_adapter_helperm_get_msg_from_helperd(void *msgs) {
    int qid = rolling_adapter_get_queue_id(0);
    if (qid == ERR) return ERR;
    return msgrcv(qid, (void *)msgs, sizeof(void*), MSG_ALL, IPC_NOWAIT);
}
int rolling_adapter_helperm_get_control_msg_from_helperd(void *msgs) {
    int qid = rolling_adapter_get_queue_id(0);
    if (qid == ERR) return ERR;
    return msgrcv(qid, (void *)msgs, sizeof(void*), MSG_CONTROL, IPC_NOWAIT);
}
int rolling_adapter_helperd_get_control_msg_from_helperm(void *msgs) {
    int qid = rolling_adapter_get_queue_id(1);
    if (qid == ERR) return ERR;
    return msgrcv(qid, (void *)msgs, sizeof(void*), MSG_CONTROL, 0);
}
int rolling_adapter_helperm_send_control_message_to_helperd(int cid, int payloadlen, char *payload_str) {
    (void)cid; (void)payloadlen; (void)payload_str;
    LOG_DEBUG("Not implemented\n");
    return ERR;
}
int rolling_adapter_helperd_send_control_message_to_helperm(int cid, int payloadlen, char *payload_str) {
    (void)cid; (void)payloadlen; (void)payload_str;
    LOG_DEBUG("Not implemented\n");
    return ERR;
}

/* ---------- 请求分析器（修改：发送响应到 resp 队列） ---------- */
static void helper_notify_request_analyzer(helper_message_struct *msgs) {
    LOG_DEBUG("Not supported yet!\n");
    if (msgs && msgs->mtext) {
        rolling_async_struct_type *mtext = (rolling_async_struct_type*)msgs->mtext;
        if (mtext->pointer) free(mtext->pointer);
        free(mtext);
        msgs->mtext = nullptr;
    }
    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (!resp_mtext) {
        LOG_ERROR("malloc failed!\n");
        return;
    }
    memset(resp_mtext, 0, sizeof(rolling_async_struct_type));
    resp_mtext->datatype = 0;
    resp_mtext->result = ERR;
    resp_mtext->pointer = nullptr;
    resp_mtext->data_size = 0;
    helper_message_struct resp_msgs;
    resp_msgs.mtype = MSG_NORMAL;
    resp_msgs.mtext = resp_mtext;
    rolling_adapter_helperm_send_msg_to_helperd(&resp_msgs, sizeof(void*));
}

static void helper_normal_request_analyzer(helper_message_struct *req_msgs) {
    int ret = ERR;
    rolling_async_struct_type *mtext_pointer = (rolling_async_struct_type*)req_msgs->mtext;
    if (!mtext_pointer) {
        LOG_ERROR("data invalid!\n");
        //free(req_msgs);
        return;
    }

    int datatype = mtext_pointer->datatype;
    int timeout = mtext_pointer->timeout;
    void *Inputdata = mtext_pointer->pointer;
    int data_size = mtext_pointer->data_size;

    LOG_DEBUG("datatype: %d\n", datatype);

    rolling_async_struct_type *resp_mtext = nullptr;
    helper_message_struct resp_msgs;

    switch (datatype) {
        case SIMDATA: {
            _SimData *OutputData = (_SimData*)malloc(sizeof(_SimData));
            if (!OutputData) {
                LOG_ERROR("malloc failed!\n");
                break;
            }
            memset(OutputData, 0, sizeof(_SimData));
            ret = HelperAfalMbimLinuxSimQuerySync(OutputData, timeout);
            if (Inputdata) free(Inputdata);
            resp_mtext = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
            if (!resp_mtext) { free(OutputData); break; }
            memset(resp_mtext, 0, sizeof(rolling_async_struct_type));
            resp_mtext->datatype = datatype;
            resp_mtext->result = ret;
            if (ret == OK) {
                resp_mtext->pointer = OutputData;
                resp_mtext->data_size = sizeof(_SimData);
            } else {
                free(OutputData);
                resp_mtext->pointer = nullptr;
                resp_mtext->data_size = 0;
            }
            resp_msgs.mtype = MSG_NORMAL;
            resp_msgs.mtext = resp_mtext;
            rolling_adapter_helperm_send_msg_to_helperd(&resp_msgs, sizeof(void*));
            break;
        }
        case RADIODATA: {
            _RadioData *OutputData = (_RadioData*)malloc(sizeof(_RadioData));
            if (!OutputData) {
                LOG_ERROR("malloc failed!\n");
                break;
            }
            memset(OutputData, 0, sizeof(_RadioData));
            if (data_size == 0) {
                ret = HelperAfalMbimLinuxRadioQuerySync(OutputData, timeout);
            } else {
                _RadioData *input = (_RadioData*)Inputdata;
                if (input && input->sw_state >= 0 && input->sw_state <= 1) {
                    OutputData->sw_state = input->sw_state;
                    ret = HelperAfalMbimLinuxRadioSetSync(OutputData, timeout);
                } else {
                    ret = ERR;
                }
                if (Inputdata) free(Inputdata);
            }
            resp_mtext = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
            if (!resp_mtext) { free(OutputData); break; }
            memset(resp_mtext, 0, sizeof(rolling_async_struct_type));
            resp_mtext->datatype = datatype;
            resp_mtext->result = ret;
            if (ret == OK) {
                resp_mtext->pointer = OutputData;
                resp_mtext->data_size = sizeof(_RadioData);
            } else {
                free(OutputData);
                resp_mtext->pointer = nullptr;
                resp_mtext->data_size = 0;
            }
            resp_msgs.mtype = MSG_NORMAL;
            resp_msgs.mtext = resp_mtext;
            rolling_adapter_helperm_send_msg_to_helperd(&resp_msgs, sizeof(void*));
            break;
        }
        case NETWORKDATA: {
            _NetworkData *OutputData = (_NetworkData*)malloc(sizeof(_NetworkData));
            if (!OutputData) { break; }
            memset(OutputData, 0, sizeof(_NetworkData));
            ret = HelperAfalMbimLinuxNetworkQuerySync(OutputData, timeout);
            if (Inputdata) free(Inputdata);
            resp_mtext = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
            if (!resp_mtext) { free(OutputData); break; }
            memset(resp_mtext, 0, sizeof(rolling_async_struct_type));
            resp_mtext->datatype = datatype;
            resp_mtext->result = ret;
            if (ret == OK) {
                resp_mtext->pointer = OutputData;
                resp_mtext->data_size = sizeof(_NetworkData);
            } else {
                free(OutputData);
                resp_mtext->pointer = nullptr;
                resp_mtext->data_size = 0;
            }
            resp_msgs.mtype = MSG_NORMAL;
            resp_msgs.mtext = resp_mtext;
            rolling_adapter_helperm_send_msg_to_helperd(&resp_msgs, sizeof(void*));
            break;
        }
        case SLOTDATA: {
            _SlotData *OutputData = (_SlotData*)malloc(sizeof(_SlotData));
            if (!OutputData) { break; }
            memset(OutputData, 0, sizeof(_SlotData));
            if (data_size == 0) {
                ret = HelperAfalMbimLinuxSlotQuerySync(OutputData, timeout);
            } else {
                _SlotData *input = (_SlotData*)Inputdata;
                if (input && input->CurrentSlotIndex >= 0 && input->CurrentSlotIndex <= 1) {
                    OutputData->CurrentSlotIndex = input->CurrentSlotIndex;
                    ret = HelperAfalMbimLinuxSlotSetSync(OutputData, timeout);
                } else {
                    ret = ERR;
                }
                if (Inputdata) free(Inputdata);
            }
            resp_mtext = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
            if (!resp_mtext) { free(OutputData); break; }
            memset(resp_mtext, 0, sizeof(rolling_async_struct_type));
            resp_mtext->datatype = datatype;
            resp_mtext->result = ret;
            if (ret == OK) {
                resp_mtext->pointer = OutputData;
                resp_mtext->data_size = sizeof(_SlotData);
            } else {
                free(OutputData);
                resp_mtext->pointer = nullptr;
                resp_mtext->data_size = 0;
            }
            resp_msgs.mtype = MSG_NORMAL;
            resp_msgs.mtext = resp_mtext;
            rolling_adapter_helperm_send_msg_to_helperd(&resp_msgs, sizeof(void*));
            break;
        }
        case ATCMDDATA: {
            char *OutputData = (char*)malloc(16 * 1024);
            if (!OutputData) {
                LOG_ERROR("malloc failed!\n");
                break;
            }
            memset(OutputData, 0, 16 * 1024);
            ret = HelperAfalMbimLinuxAtOverMbimSetSync((char*)Inputdata, OutputData, timeout);
            if (Inputdata) free(Inputdata);
            resp_mtext = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
            if (!resp_mtext) { free(OutputData); break; }
            memset(resp_mtext, 0, sizeof(rolling_async_struct_type));
            resp_mtext->datatype = datatype;
            resp_mtext->result = ret;
            if (ret == OK) {
                resp_mtext->pointer = OutputData;
                resp_mtext->data_size = strlen(OutputData) + 1;
            } else {
                free(OutputData);
                resp_mtext->pointer = nullptr;
                resp_mtext->data_size = 0;
            }
            resp_msgs.mtype = MSG_NORMAL;
            resp_msgs.mtext = resp_mtext;
            rolling_adapter_helperm_send_msg_to_helperd(&resp_msgs, sizeof(void*));
            break;
        }
        case TRACELOG: {
            _TraceLogData *OutputData = (_TraceLogData*)malloc(sizeof(_TraceLogData));
            if (!OutputData) { break; }
            memset(OutputData, 0, sizeof(_TraceLogData));
            OutputData->InputKey = (char*)malloc(1024);
            if (!OutputData->InputKey) {
                free(OutputData);
                break;
            }
            memset(OutputData->InputKey, 0, 1024);
            _TraceLogData *input = (_TraceLogData*)Inputdata;
            if (!input || !input->InputKey || strlen(input->InputKey) < 1) {
                LOG_ERROR("Data invalid!\n");
                ret = ERR;
            } else {
                strncpy(OutputData->InputKey, input->InputKey, 1023);
                OutputData->InputValue = input->InputValue;
                if (input->InputValue < 0) {
                    ret = HelperAfalMbimLinuxTraceLogQuerySync(OutputData->InputKey, &(OutputData->InputValue), timeout);
                } else {
                    ret = HelperAfalMbimLinuxTraceLogSetSync(OutputData->InputKey, &(OutputData->InputValue), timeout);
                }
            }
            if (Inputdata) free(Inputdata);
            resp_mtext = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
            if (!resp_mtext) { free(OutputData->InputKey); free(OutputData); break; }
            memset(resp_mtext, 0, sizeof(rolling_async_struct_type));
            resp_mtext->datatype = datatype;
            resp_mtext->result = ret;
            if (ret == OK) {
                resp_mtext->pointer = OutputData;
                resp_mtext->data_size = sizeof(_TraceLogData);
            } else {
                free(OutputData->InputKey);
                free(OutputData);
                resp_mtext->pointer = nullptr;
                resp_mtext->data_size = 0;
            }
            resp_msgs.mtype = MSG_NORMAL;
            resp_msgs.mtext = resp_mtext;
            rolling_adapter_helperm_send_msg_to_helperd(&resp_msgs, sizeof(void*));
            break;
        }
        default:
            LOG_ERROR("Invalid or unsupported type: %d!\n", datatype);
            if (Inputdata) free(Inputdata);
            break;
    }

    if (mtext_pointer) free(mtext_pointer);
    //free(req_msgs);
}

/* ---------- 消息接收循环（从请求队列读取） ---------- */
static void rolling_helperm_control_receiver() {
    LOG_DEBUG("ready to get control message!\n");
    helper_message_struct req_msgs;
    while (TRUE) {
        if (thread_running == ERR) {
            LOG_DEBUG("Receive request to deinit!\n");
            break;
        }
        memset(&req_msgs, 0, sizeof(req_msgs));
        int ret = rolling_adapter_helperm_get_normal_msg_from_helperd(&req_msgs);
        if (ret != OK) {
            usleep(1000);
            continue;
        }
        LOG_DEBUG("Received request, mtype=%ld\n", req_msgs.mtype);
        if (req_msgs.mtype == MSG_NORMAL)
            helper_normal_request_analyzer(&req_msgs);
        //else
            //helper_notify_request_analyzer(&req_msgs);
    }
    rolling_deinit_message_queues();
}

/* ---------- 超时恢复（只发送错误响应到响应队列） ---------- */
static void restore_caller_thread_work(gint signum) {
    LOG_ERROR("helper no resp!\n");
    recovery_flag = 1;

    helper_message_struct resp_msgs;
    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (resp_mtext) {
        memset(resp_mtext, 0, sizeof(rolling_async_struct_type));
        resp_mtext->datatype = g_datatype;
        resp_mtext->result = ERR;
        resp_mtext->pointer = nullptr;
        resp_mtext->data_size = 0;
        resp_msgs.mtype = MSG_NORMAL;
        resp_msgs.mtext = resp_mtext;
        rolling_adapter_helperm_send_msg_to_helperd(&resp_msgs, sizeof(void*));
    }
    LOG_DEBUG("Restore finished (sent error to resp queue)!\n");
}

/* ---------- 定时器 ---------- */
int rolling_adapter_helperd_timer_handle(int datatype, int timeout) {
    g_datatype = datatype;
    signal(SIGALRM, restore_caller_thread_work);
    int ret = alarm(timeout);
    if (ret != 0) LOG_DEBUG("alarm was already set\n");
    return OK;
}

int rolling_adapter_helperd_timer_close(void) {
    alarm(0);
    LOG_DEBUG("alarm closed!\n");
    return OK;
}

/* ---------- 线程入口 ---------- */
int mbim_thread_init(char *portname) {
    LOG_DEBUG("enter %s!\n", __func__);
    if (!portname) {
        LOG_ERROR("NULL pointer!\n");
        return ERR;
    }
    LOG_DEBUG("data:%s\n", portname);

    int ret = rolling_adapter_helper_queue_init();
    if (ret != OK) {
        LOG_ERROR("init queues failed!\n");
        return ERR;
    }

    ret = HelperLinuxMbimDeviceOpen(portname);
    if (ret != OK) {
        LOG_ERROR("can't init mbim device!\n");
        return ERR;
    }

    pthread_mutex_lock(&g_loop_mutex);
    g_msg_loop_ready = 1;
    pthread_cond_broadcast(&g_loop_cond);
    pthread_mutex_unlock(&g_loop_mutex);

    rolling_helperm_control_receiver();

    LOG_DEBUG("Thread will exit!\n");
    return OK;
}

int mbim_thread_deinit(void) {
    if (thread_running == OK) {
        thread_running = ERR;
    } else {
        LOG_DEBUG("No existed thread!\n");
    }
    int ret = HelperLinuxMbimDeviceClose();
    if (ret != OK) {
        LOG_ERROR("can't deinit mbim device!\n");
        return ERR;
    }
    recovery_flag = 0;
    rolling_deinit_message_queues();
    return OK;
}

/* ---------- 对外同步函数 ---------- */
void wait_for_loop_ready(void) {
    pthread_mutex_lock(&g_loop_mutex);
    while (!g_msg_loop_ready) {
        pthread_cond_wait(&g_loop_cond, &g_loop_mutex);
    }
    pthread_mutex_unlock(&g_loop_mutex);
}