#ifndef MBIM_LINUX_HELPER_ADAPTER_H
#define MBIM_LINUX_HELPER_ADAPTER_H

#include <signal.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int datatype;
    int timeout;
    int result;
    int data_size;
    void *pointer;
} rolling_async_struct_type;

typedef struct {
    long   mtype;     // 消息类型，必须 > 0
    void   *mtext;    // 指向 heap 分配的 rolling_async_struct_type
} helper_message_struct;

typedef enum {
    HELPERD_INPUT = 0,
    HELPERD_OUTPUT = 1,  // 新增：响应队列标识
} queue_enum_type;

typedef enum {
    MSG_ALL     = 0,
    MSG_NORMAL  = 1,
    MSG_CONTROL = 2
} seq_message_enum_type;

// 接收循环就绪标志
extern volatile sig_atomic_t g_msg_loop_ready;

// 等待接收循环就绪（阻塞）
void wait_for_loop_ready(void);

// 原有函数声明（保持接口不变，内部实现调整）
int rolling_adapter_helperd_timer_handle(int datatype, int timeout);
int rolling_adapter_helperd_timer_close(void);
int rolling_adapter_helperm_send_control_message_to_helperd(int cid, int payloadlen, char *payload_str);
int rolling_adapter_helperd_send_control_message_to_helperm(int cid, int payloadlen, char *payload_str);
int rolling_adapter_helperm_get_msg_from_helperd(void *msgs);
int rolling_adapter_helperm_get_control_msg_from_helperd(void *msgs);
int rolling_adapter_helperm_get_normal_msg_from_helperd(void *msgs);
int rolling_adapter_helperm_send_msg_to_helperd(void *msgs, int msgsize);
int rolling_adapter_helperd_get_control_msg_from_helperm(void *msgs);
int rolling_adapter_helperd_get_normal_msg_from_helperm(void *msgs);
int rolling_adapter_helperd_send_req_to_helperm(void *msgs, int msgsize);
int mbim_thread_init(char *portname);
int mbim_thread_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // MBIM_LINUX_HELPER_ADAPTER_H
