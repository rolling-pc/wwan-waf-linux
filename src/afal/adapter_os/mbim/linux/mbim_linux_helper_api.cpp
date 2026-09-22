#include "log.hpp"
#include "common.hpp"
#include "mbim_linux_helper_adapter.h"
#include "mbim_linux_helper_api.h"
#include "libmbim_common_struct.h"
#include <thread>
#include <mutex>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#define SEM_NAME "/waf_mbim_semaphore"

using namespace afal::log;
using namespace afal::error;

bool g_mbim_device_init_flag = false;
extern int thread_running;
std::mutex g_mutex;

/* ------------------- 辅助检查函数（原样） ------------------- */
bool likely_valid_pointer(void* ptr) {
    if (ptr == nullptr) return false;
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    if (addr < 0x1000 || addr > 0xFFFFFFFFFFFF) return false;
    if (addr % 8 != 0) return false;
    return true;
}

/* ------------------- 所有 API 函数（堆分配发送请求） ------------------- */

int AfalMbimLinuxSimQuerySync(char *portname, _SimData *OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    if (timeout <= 0 || !OutputData || !g_mbim_device_init_flag) {
        LOG_ERROR("illegal input param or MBIM not init!\n");
        return ERR;
    }
    (void)portname; // 未使用

    // 堆分配请求结构
    helper_message_struct *req_msgs = (helper_message_struct*)malloc(sizeof(helper_message_struct));
    if (!req_msgs) { LOG_ERROR("malloc failed\n"); return ERR; }
    memset(req_msgs, 0, sizeof(helper_message_struct));

    rolling_async_struct_type *user_data = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (!user_data) { free(req_msgs); return ERR; }
    memset(user_data, 0, sizeof(rolling_async_struct_type));

    user_data->datatype = SIMDATA;
    user_data->timeout = timeout;
    user_data->data_size = 0;
    user_data->pointer = nullptr;   // 无输入数据

    req_msgs->mtype = MSG_NORMAL;
    req_msgs->mtext = user_data;

    int ret = rolling_adapter_helperd_send_req_to_helperm(req_msgs, sizeof(void*));
    if (ret != OK) {
        free(user_data);
        free(req_msgs);
        return ret;
    }

    rolling_adapter_helperd_timer_handle(SIMDATA, timeout);
    helper_message_struct resp_msgs;
    ret = rolling_adapter_helperd_get_normal_msg_from_helperm(&resp_msgs);
    rolling_adapter_helperd_timer_close();
    if (ret != OK) {
        LOG_ERROR("get resp failed\n");
        return ERR;
    }

    if (!resp_msgs.mtext) {
        LOG_ERROR("empty response\n");
        return ERR;
    }

    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)resp_msgs.mtext;
    if (resp_mtext->result != OK || !resp_mtext->pointer) {
        ret = ERR;
        if (resp_mtext->pointer) free(resp_mtext->pointer);
        free(resp_mtext);
        return ret;
    }

    _SimData *data = (_SimData*)resp_mtext->pointer;
    memcpy(OutputData, data, sizeof(_SimData));
    free(data);
    free(resp_mtext);
    return OK;
}

int AfalMbimLinuxNetworkQuerySync(char *portname, _NetworkData *OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    if (timeout <= 0 || !OutputData || !g_mbim_device_init_flag) {
        LOG_ERROR("illegal input param!\n");
        return ERR;
    }
    (void)portname;

    helper_message_struct *req_msgs = (helper_message_struct*)malloc(sizeof(helper_message_struct));
    if (!req_msgs) return ERR;
    memset(req_msgs, 0, sizeof(helper_message_struct));

    rolling_async_struct_type *user_data = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (!user_data) { free(req_msgs); return ERR; }
    memset(user_data, 0, sizeof(rolling_async_struct_type));

    user_data->datatype = NETWORKDATA;
    user_data->timeout = timeout;
    user_data->data_size = 0;
    user_data->pointer = nullptr;

    req_msgs->mtype = MSG_NORMAL;
    req_msgs->mtext = user_data;

    int ret = rolling_adapter_helperd_send_req_to_helperm(req_msgs, sizeof(void*));
    if (ret != OK) {
        free(user_data);
        free(req_msgs);
        return ret;
    }

    rolling_adapter_helperd_timer_handle(NETWORKDATA, timeout);
    helper_message_struct resp_msgs;
    ret = rolling_adapter_helperd_get_normal_msg_from_helperm(&resp_msgs);
    rolling_adapter_helperd_timer_close();
    if (ret != OK) { LOG_ERROR("get resp failed\n"); return ERR; }

    if (!resp_msgs.mtext) return ERR;

    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)resp_msgs.mtext;
    if (resp_mtext->result != OK || !resp_mtext->pointer) {
        ret = ERR;
        if (resp_mtext->pointer) free(resp_mtext->pointer);
        free(resp_mtext);
        return ret;
    }

    _NetworkData *data = (_NetworkData*)resp_mtext->pointer;
    memcpy(OutputData, data, sizeof(_NetworkData));
    free(data);
    free(resp_mtext);
    return OK;
}

int AfalMbimLinuxRadioQuerySync(char *portname, _RadioData *OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    if (timeout <= 0 || !OutputData || !g_mbim_device_init_flag) {
        LOG_ERROR("illegal input param!\n");
        return ERR;
    }
    (void)portname;

    helper_message_struct *req_msgs = (helper_message_struct*)malloc(sizeof(helper_message_struct));
    if (!req_msgs) return ERR;
    memset(req_msgs, 0, sizeof(helper_message_struct));

    rolling_async_struct_type *user_data = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (!user_data) { free(req_msgs); return ERR; }
    memset(user_data, 0, sizeof(rolling_async_struct_type));

    user_data->datatype = RADIODATA;
    user_data->timeout = timeout;
    user_data->data_size = 0;
    user_data->pointer = nullptr;

    req_msgs->mtype = MSG_NORMAL;
    req_msgs->mtext = user_data;

    int ret = rolling_adapter_helperd_send_req_to_helperm(req_msgs, sizeof(void*));
    if (ret != OK) {
        free(user_data);
        free(req_msgs);
        return ret;
    }

    rolling_adapter_helperd_timer_handle(RADIODATA, timeout);
    helper_message_struct resp_msgs;
    ret = rolling_adapter_helperd_get_normal_msg_from_helperm(&resp_msgs);
    rolling_adapter_helperd_timer_close();
    if (ret != OK) { LOG_ERROR("get resp failed\n"); return ERR; }

    if (!resp_msgs.mtext) return ERR;

    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)resp_msgs.mtext;
    if (resp_mtext->result != OK || !resp_mtext->pointer) {
        ret = ERR;
        if (resp_mtext->pointer) free(resp_mtext->pointer);
        free(resp_mtext);
        return ret;
    }

    _RadioData *data = (_RadioData*)resp_mtext->pointer;
    memcpy(OutputData, data, sizeof(_RadioData));
    free(data);
    free(resp_mtext);
    return OK;
}

int AfalMbimLinuxSlotQuerySync(char *portname, _SlotData *OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    if (timeout <= 0 || !OutputData || !g_mbim_device_init_flag) {
        LOG_ERROR("illegal input param!\n");
        return ERR;
    }
    (void)portname;

    helper_message_struct *req_msgs = (helper_message_struct*)malloc(sizeof(helper_message_struct));
    if (!req_msgs) return ERR;
    memset(req_msgs, 0, sizeof(helper_message_struct));

    rolling_async_struct_type *user_data = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (!user_data) { free(req_msgs); return ERR; }
    memset(user_data, 0, sizeof(rolling_async_struct_type));

    user_data->datatype = SLOTDATA;
    user_data->timeout = timeout;
    user_data->data_size = 0;
    user_data->pointer = nullptr;

    req_msgs->mtype = MSG_NORMAL;
    req_msgs->mtext = user_data;

    int ret = rolling_adapter_helperd_send_req_to_helperm(req_msgs, sizeof(void*));
    if (ret != OK) {
        free(user_data);
        free(req_msgs);
        return ret;
    }

    rolling_adapter_helperd_timer_handle(SLOTDATA, timeout);
    helper_message_struct resp_msgs;
    ret = rolling_adapter_helperd_get_normal_msg_from_helperm(&resp_msgs);
    rolling_adapter_helperd_timer_close();
    if (ret != OK) { LOG_ERROR("get resp failed\n"); return ERR; }

    if (!resp_msgs.mtext) return ERR;

    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)resp_msgs.mtext;
    if (resp_mtext->result != OK || !resp_mtext->pointer) {
        ret = ERR;
        if (resp_mtext->pointer) free(resp_mtext->pointer);
        free(resp_mtext);
        return ret;
    }

    _SlotData *data = (_SlotData*)resp_mtext->pointer;
    memcpy(OutputData, data, sizeof(_SlotData));
    free(data);
    free(resp_mtext);
    return OK;
}

int AfalMbimLinuxTraceLogQuerySync(char *portname, char *InputKey, int *OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    if (timeout <= 0 || !InputKey || !OutputData || !g_mbim_device_init_flag) {
        LOG_ERROR("illegal input param!\n");
        return ERR;
    }
    (void)portname;

    helper_message_struct *req_msgs = (helper_message_struct*)malloc(sizeof(helper_message_struct));
    if (!req_msgs) return ERR;
    memset(req_msgs, 0, sizeof(helper_message_struct));

    rolling_async_struct_type *user_data = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (!user_data) { free(req_msgs); return ERR; }
    memset(user_data, 0, sizeof(rolling_async_struct_type));

    // 构造输入数据
    _TraceLogData *input = (_TraceLogData*)malloc(sizeof(_TraceLogData));
    if (!input) { free(user_data); free(req_msgs); return ERR; }
    memset(input, 0, sizeof(_TraceLogData));
    input->InputKey = strdup(InputKey);
    if (!input->InputKey) { free(input); free(user_data); free(req_msgs); return ERR; }
    input->InputValue = -1;  // 查询

    user_data->datatype = TRACELOG;
    user_data->timeout = timeout;
    user_data->data_size = sizeof(_TraceLogData);
    user_data->pointer = input;

    req_msgs->mtype = MSG_NORMAL;
    req_msgs->mtext = user_data;

    int ret = rolling_adapter_helperd_send_req_to_helperm(req_msgs, sizeof(void*));
    if (ret != OK) {
        free(input->InputKey);
        free(input);
        free(user_data);
        free(req_msgs);
        return ret;
    }

    rolling_adapter_helperd_timer_handle(TRACELOG, timeout);
    helper_message_struct resp_msgs;
    ret = rolling_adapter_helperd_get_normal_msg_from_helperm(&resp_msgs);
    rolling_adapter_helperd_timer_close();
    if (ret != OK) { LOG_ERROR("get resp failed\n"); return ERR; }

    if (!resp_msgs.mtext) return ERR;

    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)resp_msgs.mtext;
    if (resp_mtext->result != OK || !resp_mtext->pointer) {
        ret = ERR;
        if (resp_mtext->pointer) free(resp_mtext->pointer);
        free(resp_mtext);
        return ret;
    }

    _TraceLogData *data = (_TraceLogData*)resp_mtext->pointer;
    *OutputData = data->InputValue;
    free(data->InputKey);
    free(data);
    free(resp_mtext);
    return OK;
}

int AfalMbimLinuxAtOverMbimSetSync(char *portname, char *input_str, char *output_str, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    if (timeout <= 0 || !input_str || !output_str || !g_mbim_device_init_flag) {
        LOG_ERROR("illegal input param!:%d-%d-%p-%p\n", timeout, g_mbim_device_init_flag, input_str, output_str);
        return ERR;
    }
    (void)portname;

    helper_message_struct *req_msgs = (helper_message_struct*)malloc(sizeof(helper_message_struct));
    if (!req_msgs) return ERR;
    memset(req_msgs, 0, sizeof(helper_message_struct));

    rolling_async_struct_type *user_data = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (!user_data) { free(req_msgs); return ERR; }
    memset(user_data, 0, sizeof(rolling_async_struct_type));

    char *input_copy = strdup(input_str);
    if (!input_copy) { free(user_data); free(req_msgs); return ERR; }

    user_data->datatype = ATCMDDATA;
    user_data->timeout = timeout;
    user_data->data_size = strlen(input_str);
    user_data->pointer = input_copy;

    req_msgs->mtype = MSG_NORMAL;
    req_msgs->mtext = user_data;

    int ret = rolling_adapter_helperd_send_req_to_helperm(req_msgs, sizeof(void*));
    if (ret != OK) {
        free(input_copy);
        free(user_data);
        free(req_msgs);
        return ret;
    }

    rolling_adapter_helperd_timer_handle(ATCMDDATA, timeout);
    helper_message_struct resp_msgs;
    ret = rolling_adapter_helperd_get_normal_msg_from_helperm(&resp_msgs);
    rolling_adapter_helperd_timer_close();
    if (ret != OK) { LOG_ERROR("get resp failed\n"); return ERR; }

    if (!resp_msgs.mtext) return ERR;

    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)resp_msgs.mtext;
    if (resp_mtext->result != OK || !resp_mtext->pointer) {
        ret = ERR;
        if (resp_mtext->pointer) {
            char *errstr = (char*)resp_mtext->pointer;
            strncpy(output_str, errstr, 16*1024 - 1);
            free(errstr);
        }
        free(resp_mtext);
        return ret;
    }

    char *data = (char*)resp_mtext->pointer;
    strncpy(output_str, data, 16*1024 - 1);
    free(data);
    free(resp_mtext);
    return OK;
}

int AfalMbimLinuxSlotSetSync(char *portname, _SlotData *InputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    if (timeout <= 0 || !InputData || !g_mbim_device_init_flag) {
        LOG_ERROR("illegal input param!\n");
        return ERR;
    }
    (void)portname;

    helper_message_struct *req_msgs = (helper_message_struct*)malloc(sizeof(helper_message_struct));
    if (!req_msgs) return ERR;
    memset(req_msgs, 0, sizeof(helper_message_struct));

    rolling_async_struct_type *user_data = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (!user_data) { free(req_msgs); return ERR; }
    memset(user_data, 0, sizeof(rolling_async_struct_type));

    _SlotData *input_copy = (_SlotData*)malloc(sizeof(_SlotData));
    if (!input_copy) { free(user_data); free(req_msgs); return ERR; }
    memcpy(input_copy, InputData, sizeof(_SlotData));

    user_data->datatype = SLOTDATA;
    user_data->timeout = timeout;
    user_data->data_size = sizeof(_SlotData);
    user_data->pointer = input_copy;

    req_msgs->mtype = MSG_NORMAL;
    req_msgs->mtext = user_data;

    int ret = rolling_adapter_helperd_send_req_to_helperm(req_msgs, sizeof(void*));
    if (ret != OK) {
        free(input_copy);
        free(user_data);
        free(req_msgs);
        return ret;
    }

    rolling_adapter_helperd_timer_handle(SLOTDATA, timeout);
    helper_message_struct resp_msgs;
    ret = rolling_adapter_helperd_get_normal_msg_from_helperm(&resp_msgs);
    rolling_adapter_helperd_timer_close();
    if (ret != OK) { LOG_ERROR("get resp failed\n"); return ERR; }

    if (!resp_msgs.mtext) return ERR;

    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)resp_msgs.mtext;
    if (resp_mtext->result != OK || !resp_mtext->pointer) {
        ret = ERR;
        if (resp_mtext->pointer) free(resp_mtext->pointer);
        free(resp_mtext);
        return ret;
    }

    _SlotData *data = (_SlotData*)resp_mtext->pointer;
    memcpy(InputData, data, sizeof(_SlotData));
    free(data);
    free(resp_mtext);
    return OK;
}

int AfalMbimLinuxRadioSetSync(char *portname, _RadioData *InputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    if (timeout <= 0 || !InputData || !g_mbim_device_init_flag) {
        LOG_ERROR("illegal input param!\n");
        return ERR;
    }
    (void)portname;

    helper_message_struct *req_msgs = (helper_message_struct*)malloc(sizeof(helper_message_struct));
    if (!req_msgs) return ERR;
    memset(req_msgs, 0, sizeof(helper_message_struct));

    rolling_async_struct_type *user_data = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (!user_data) { free(req_msgs); return ERR; }
    memset(user_data, 0, sizeof(rolling_async_struct_type));

    _RadioData *input_copy = (_RadioData*)malloc(sizeof(_RadioData));
    if (!input_copy) { free(user_data); free(req_msgs); return ERR; }
    memcpy(input_copy, InputData, sizeof(_RadioData));

    user_data->datatype = RADIODATA;
    user_data->timeout = timeout;
    user_data->data_size = sizeof(_RadioData);
    user_data->pointer = input_copy;

    req_msgs->mtype = MSG_NORMAL;
    req_msgs->mtext = user_data;

    int ret = rolling_adapter_helperd_send_req_to_helperm(req_msgs, sizeof(void*));
    if (ret != OK) {
        free(input_copy);
        free(user_data);
        free(req_msgs);
        return ret;
    }

    rolling_adapter_helperd_timer_handle(RADIODATA, timeout);
    helper_message_struct resp_msgs;
    ret = rolling_adapter_helperd_get_normal_msg_from_helperm(&resp_msgs);
    rolling_adapter_helperd_timer_close();
    if (ret != OK) { LOG_ERROR("get resp failed\n"); return ERR; }

    if (!resp_msgs.mtext) return ERR;

    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)resp_msgs.mtext;
    if (resp_mtext->result != OK || !resp_mtext->pointer) {
        ret = ERR;
        if (resp_mtext->pointer) free(resp_mtext->pointer);
        free(resp_mtext);
        return ret;
    }

    _RadioData *data = (_RadioData*)resp_mtext->pointer;
    memcpy(InputData, data, sizeof(_RadioData));
    free(data);
    free(resp_mtext);
    return OK;
}

int AfalMbimLinuxTraceLogSetSync(char *portname, char *InputStr, int *InputValue, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    if (timeout <= 0 || !InputStr || !InputValue || !g_mbim_device_init_flag) {
        LOG_ERROR("illegal input param!\n");
        return ERR;
    }
    (void)portname;

    helper_message_struct *req_msgs = (helper_message_struct*)malloc(sizeof(helper_message_struct));
    if (!req_msgs) return ERR;
    memset(req_msgs, 0, sizeof(helper_message_struct));

    rolling_async_struct_type *user_data = (rolling_async_struct_type*)malloc(sizeof(rolling_async_struct_type));
    if (!user_data) { free(req_msgs); return ERR; }
    memset(user_data, 0, sizeof(rolling_async_struct_type));

    _TraceLogData *input = (_TraceLogData*)malloc(sizeof(_TraceLogData));
    if (!input) { free(user_data); free(req_msgs); return ERR; }
    memset(input, 0, sizeof(_TraceLogData));
    input->InputKey = strdup(InputStr);
    if (!input->InputKey) { free(input); free(user_data); free(req_msgs); return ERR; }
    input->InputValue = *InputValue;

    user_data->datatype = TRACELOG;
    user_data->timeout = timeout;
    user_data->data_size = sizeof(_TraceLogData);
    user_data->pointer = input;

    req_msgs->mtype = MSG_NORMAL;
    req_msgs->mtext = user_data;

    int ret = rolling_adapter_helperd_send_req_to_helperm(req_msgs, sizeof(void*));
    if (ret != OK) {
        free(input->InputKey);
        free(input);
        free(user_data);
        free(req_msgs);
        return ret;
    }

    rolling_adapter_helperd_timer_handle(TRACELOG, timeout);
    helper_message_struct resp_msgs;
    ret = rolling_adapter_helperd_get_normal_msg_from_helperm(&resp_msgs);
    rolling_adapter_helperd_timer_close();
    if (ret != OK) { LOG_ERROR("get resp failed\n"); return ERR; }

    if (!resp_msgs.mtext) return ERR;

    rolling_async_struct_type *resp_mtext = (rolling_async_struct_type*)resp_msgs.mtext;
    if (resp_mtext->result != OK || !resp_mtext->pointer) {
        ret = ERR;
        if (resp_mtext->pointer) free(resp_mtext->pointer);
        free(resp_mtext);
        return ret;
    }

    _TraceLogData *data = (_TraceLogData*)resp_mtext->pointer;
    *InputValue = data->InputValue;
    free(data->InputKey);
    free(data);
    free(resp_mtext);
    return OK;
}

/* ------------------- 设备打开/关闭（增加同步等待） ------------------- */
int LinuxMbimDeviceOpen(const char *InputPortName) {
    LOG_DEBUG("enter %s!\n", __func__);
    int ret = ERR;
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        LOG_ERROR("sem_open failed: %s\n", strerror(errno));
        return ERR;
    }

    auto portname = std::shared_ptr<char>(new char[64], std::default_delete<char[]>());
    memset(portname.get(), 0, 64);
    strncpy(portname.get(), InputPortName, 63);

    sem_wait(sem);
    std::unique_lock<std::mutex> lock(g_mutex);
    if (thread_running != OK) {
        thread_running = OK;
        std::thread mbim_send_thread(mbim_thread_init, portname.get());
        mbim_send_thread.detach();

        int retryflag = 0;
        while(!g_mbim_device_init_flag && retryflag < 30) {
            LOG_DEBUG("wait for mbim init!\n");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            retryflag++;
        }
        if (!g_mbim_device_init_flag) {
            LOG_ERROR("mbim init failed finally!\n");
            ret = ERR;
        } else {
            // 等待接收循环就绪
            wait_for_loop_ready();
            LOG_DEBUG("message loop is ready\n");
            ret = OK;
        }
    } else {
        LOG_DEBUG("already a existed thread!\n");
        ret = OK;
    }
    sem_post(sem);
    sem_close(sem);
    return ret;
}

int LinuxMbimDeviceClose() {
    LOG_DEBUG("enter %s!\n", __func__);
    return mbim_thread_deinit();
}