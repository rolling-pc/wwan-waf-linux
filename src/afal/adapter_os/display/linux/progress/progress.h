#ifndef SRC_AFAL_ADAPTER_OS_DISPLAY_LINUX_PROGRESS_PROGRESS_H_
#define SRC_AFAL_ADAPTER_OS_DISPLAY_LINUX_PROGRESS_PROGRESS_H_

#include <stdio.h>

#define RDONLY                           "r"
#define WRONLY                           "w"

typedef enum
{
    RET_ERROR        = -1,
    RET_ERR_INTERNAL = RET_ERROR,
    RET_MIN          = RET_ERROR,
    RET_OK           = 0,
    RET_ERR_PROCESS,
    RET_ERR_BUSY,
    RET_ERR_RESOURCE,
    RET_MAX
}fibo_ret_enum_type;

/*progress*/
//枚举元素的命名不能随意更改，需要和DISTRIB_ID完全一致
enum CurrentDistibId {
    None = 0,
    Ubuntu,
    Thinpro,
    Fedora,
};
typedef struct Progress Progress;
// 父类结构体
struct Progress {
    int progressWidth; 
    int progressHeight;
    FILE * progressFd;
    FILE * progressCloseFd;

    char progressType[32];
    char progressSchedule[32];
    char progressId[32];
    char progressTitle[64];
    char progressText[256];
    char environmentVariable[256];
    char progressCmd[1024];
    char progressCloseCmd[512];

    // 父类方法指针
    // 获取环境变量
    int (*fibocom_get_progress_environment_variable)(Progress *self);
    // 执行进度条命令
    int (*fibocom_start_progress)(Progress *self);
    // 更新进度条标题
    int (*fibocom_set_progress_title)(Progress *self, const char* title);
    // 初始进度条内容
    int (*fibocom_set_progress_init_text)(Progress *self);
    // 更新进度条内容
    int (*fibocom_set_progress_text)(Progress *self, const char* text);
    // 更新进度条进度
    int (*fibocom_set_progress_schedule)(Progress *self, int schedule);
    // 刷新进度条
    int (*fibocom_refresh_progress)(Progress *progress, const char *text, int schedule);
    // 关闭当前进度条
    int (*fibocom_close_progress)(Progress *self);
};

/*end of progress*/

int fibocom_get_current_distrib_id(enum CurrentDistibId *hostType);
Progress *CreateProgressImpl(enum CurrentDistibId hostType);
void DestroyProgressImpl(Progress *self);

#endif  // AFAL_ADAPTER_OS_DISPLAY_LINUX_PROGRESS_PROGRESS_H_
