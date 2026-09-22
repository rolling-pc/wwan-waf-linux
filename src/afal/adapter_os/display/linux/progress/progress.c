#include "progress.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
char* itoa(int num,char* str,int radix)
{
    char index[]="0123456789ABCDEF";
    unsigned unum;
    int i=0,j,k;

    if(radix==10&&num<0){
        unum=(unsigned)-num;
        str[i++]='-';
    }
    else unum=(unsigned)num;

    do{
        str[i++]=index[unum%(unsigned)radix];
        unum/=radix;
    }while(unum);

    str[i]='\0';
    if(str[0]=='-')
        k=1;
    else
        k=0;

    for(j=k;j<=(i-1)/2;j++){
        char temp;
        temp=str[j];
        str[j]=str[i-1+k-j];
        str[i-1+k-j]=temp;
    }
    return str;
}

/*progress start*/
//zenity
void fibocom_remove_last_newline(char *str) {
    int len = strlen(str);
    int last_newline = len - 1;
    while (last_newline >= 0 && (str[last_newline] == '\n' || str[last_newline] == '\r')) {
        last_newline--;
    }
    str[last_newline + 1] = '\0';
}
static int
find_fedora_file() {
    char get_current_distrib_id_cmd[] = "cat /etc/fedora-release | grep Fedora";
    FILE *fd                          = NULL;
    char distrib_id[64]               = {0};
    int  ret                          = 0;
    char *env_resp                    = NULL;
    fd = popen(get_current_distrib_id_cmd, "r");
    if(fd == NULL) {
        printf("get_current_distrib_id_fd error!\n");
        return RET_ERROR;
    }
    ret = fread(distrib_id, sizeof(char), 64, fd);
    fclose(fd);
    if(ret == RET_ERROR || strlen(distrib_id) < 1){
        printf("read error!\n");
        return RET_ERROR;
    }
    return RET_OK;
}
static int
init_env_for_wayland(Progress *progress, int uid, char *username)
{
    printf("enter!\n");
    char environment_string[]                      = "export XDG_CURRENT_DESKTOP=\"ubuntu:GNOME\"\nexport XDG_RUNTIME_DIR=";
    char fedora_environment_string[]               = "export XDG_CURRENT_DESKTOP=\"GNOME\"\nexport XDG_RUNTIME_DIR=";
    char environment_temp[64]                      = {0};
    char get_display_env_result[128]               = {0};
    if (progress == NULL) {
        printf("NULL pointer!\n");
        return RET_ERROR;
    }
    if (uid < 1000) {
        printf("Invalid uid:%d!\n", uid);
        return RET_ERROR;
    }
    // step1: get active user's XDG_RUNTIME_DIR variable.
    snprintf(environment_temp, 64, "/run/user/%d/\n", uid);
    printf("XDG_RUNTIME_DIR value:%s", environment_temp);
    if (find_fedora_file() == RET_OK) {
        strncpy(progress->environmentVariable, fedora_environment_string, strlen(fedora_environment_string));
    }
    else {
        strncpy(progress->environmentVariable, environment_string, strlen(environment_string));
    }
    strncat(progress->environmentVariable, environment_temp, 64);
    // step2: set corresponding WAYLAND_DISPLAY variables.
    // on test machine, all two login user's WAYLAND_DISPLAY are all "wayland-0".
    snprintf(get_display_env_result, 128, "export WAYLAND_DISPLAY=\"wayland-0\"\n");
    strncat(progress->environmentVariable, get_display_env_result, 35);
    return RET_OK;
}
static int
init_env_for_x11(Progress *progress, int uid, char *username)
{
    printf("enter!\n");
    int  ret                                       = RET_ERROR;
    char environment_temp[64]                      = {0};
    char environment_string[]                      = "export XDG_CURRENT_DESKTOP=\"ubuntu:GNOME\"\nexport XAUTHORITY=";
    char fedora_environment_string[]               = "export XDG_CURRENT_DESKTOP=\"GNOME\"\nexport XAUTHORITY=";
    FILE *fp                                       = NULL;
    char command[128]                              = {0};
    char pid_str[32]                               = {0};
    int  pid                                       = RET_ERROR;
    char path[128]                                 = {0};
    char *buffer                                   = NULL;
    int  len                                       = 0;
    char *start                                    = NULL;
    char valid_display_value[16]                   = {0};
    char get_display_env_result[128]               = {0};
    if (progress == NULL) {
        printf("NULL pointer!\n");
        return RET_ERROR;
    }
    if (uid < 1000 || username == NULL || strlen(username) < 1) {
        printf("Invalid param!\n");
        return RET_ERROR;
    }
    // step1: get current user's xauthority variable.
    // fedora don't contain the gdm folder on run/user/uid path.
    snprintf(environment_temp, 64, "find /run/user/%d/gdm/ -name Xauthority 2>/dev/null\n", uid);
    fp = popen(environment_temp, RDONLY);
    if (fp == NULL) {
        printf("open fp failed!\n");
        return RET_ERROR;
    }
    memset(environment_temp, 0, 64);
    while (fgets(environment_temp, sizeof(environment_temp), fp) != NULL) {
        printf("Xauthority value:%s\n", environment_temp);
    }
    pclose(fp);
    // add logic to check whether xauthority is existed.
    if (strlen(environment_temp) > 1) {
        printf("Xauthority file existed!\n");
        memset(environment_temp, 0, 64);
        snprintf(environment_temp, 64, "/run/user/%d/gdm/Xauthority\n", uid);
    }
    else {
        printf("Xauthority file not existed, will try to find specific file instead!\n");
        // if not, we should find and use specific name instead.
        memset(environment_temp, 0, 64);
        snprintf(environment_temp, 64, "find /run/user/%d/ -name *Xwayland* 2>/dev/null", uid);
        fp = popen(environment_temp, RDONLY);
        if (fp == NULL) {
            printf("open fp failed!\n");
            return RET_ERROR;
        }
        memset(environment_temp, 0, 64);
        while (fgets(environment_temp, sizeof(environment_temp), fp) != NULL) {
            printf("Specific Xauthority value:%s\n", environment_temp);
        }
        pclose(fp);
    }
    if (!find_fedora_file()) {
        strncpy(progress->environmentVariable, fedora_environment_string, strlen(fedora_environment_string));
    }
    else {
        strncpy(progress->environmentVariable, environment_string, strlen(environment_string));
    }
    strncat(progress->environmentVariable, environment_temp, 64);
    // step2: try to get DISPLAY value from active user's screensaver thread.
    // only X11 support xhost and DISPLAY variable by default.
    snprintf(command, sizeof(command), "ps -ef | grep '[o]rg.gnome.ScreenSaver' | awk '$1 == \"%s\" {print $2}'", username);
    fp = popen(command, RDONLY);
    if (fp == NULL) {
        printf("open fp failed!\n");
        return RET_ERROR;
    }
    while (fgets(pid_str, sizeof(pid_str), fp) != NULL) {
        pid = atoi(pid_str);
        printf("find pid: %d\n", pid);
    }
    pclose(fp);
    if (pid == RET_ERROR) {
        printf("can't find pid!\n");
        return RET_ERROR;
    }
    snprintf(path, sizeof(path), "/proc/%d/environ", pid);
    fp = fopen(path, "r");
    if (fp == NULL) {
        printf("open /proc/%d/environ failed!\n", pid);
        return RET_ERROR;
    }
    buffer = malloc(4 * 1024 * sizeof(char));
    if (buffer == NULL) {
        printf("malloc space failed!\n");
        return RET_ERROR;
    }
    memset(buffer, 0, 4 * 1024 * sizeof(char));
    len = fread(buffer, 1, 4 * 1024 * sizeof(char), fp);
    fclose(fp);
    // all environs will be divided with "\0", aka we can't get value directly by strstr or strlen!
    if (len == 0) {
        printf("read /proc/%d/environ failed!\n", pid);
        free(buffer);
        buffer = NULL;
        return RET_ERROR;
    }
    start = buffer;
    while (start < buffer + len) {
        // printf("current env:%s\n", start);
        if (strncmp(start, "DISPLAY=", 8) == 0) {
            strncpy(valid_display_value, (start + 8), strlen(start + 8));
            break;
        }
        start += strlen(start) + 1;
    }
    if (strlen(valid_display_value) < 1) {
        printf("Can't find DISPLAY value!\n");
        free(buffer);
        buffer = NULL;
        return RET_ERROR;
    }
    printf("DISPLAY: %s\n", valid_display_value);
    free(buffer);
    buffer = NULL;
    snprintf(get_display_env_result, 128, "\n export DISPLAY=\"%s\"\n", valid_display_value);
    strncat(progress->environmentVariable, get_display_env_result, 128);
    return RET_OK;
}
static int
find_active_user_id(int *uid, char *username, int *windowing_system)
{
    FILE *fd                       = NULL;
    char get_login_session_id[]    = "loginctl list-sessions --no-legend | awk '{print $1}'";
    char resp[16]                  = {0};
    int  session_id                = RET_ERROR;
    int  num_sessions              = 0;
    int  sessions[10]              = {0};  // we consider that there is no way that 10 user logined at same time!
    char get_active_session_id[64] = {0};
    int  ret                       = RET_ERROR;
    int  valid_session_id          = 0;
    if (uid == NULL) {
        printf("NULL pointer!\n");
        return RET_ERROR;
    }
    *uid = 0;
    fd = popen(get_login_session_id, RDONLY);
    if (NULL == fd) {
        printf("open fd error\n");
        return RET_ERROR;
    }
    while (fgets(resp, sizeof(resp), fd) != NULL) {
        session_id = atoi(resp);
        if (session_id > 0 && num_sessions < 10) {
            sessions[num_sessions++] = session_id;
        }
    }
    pclose(fd);
    if (num_sessions < 1) {
        printf("No user logined! will use default value!\n");
        *uid = 1000;
    } else {
        for (int i = 0; i < num_sessions; i++) {
            snprintf(get_active_session_id, 64, "loginctl show-session %d -p State --value", sessions[i]);
            fd = popen(get_active_session_id, RDONLY);
            if (NULL == fd) {
                printf("open fd error\n");
                return RET_ERROR;
            }
            ret = fread(resp, sizeof(char), sizeof(resp), fd);
            pclose(fd);
            if (!ret || strlen(resp) < 1 || strstr(resp, "active") == NULL) {
                // printf("No active user, continue to find!\n");
                continue;
            } else {
                valid_session_id = sessions[i];
            }
            snprintf(get_active_session_id, 64, "loginctl show-session %d -p User --value", valid_session_id);
            fd = popen(get_active_session_id, RDONLY);
            if (NULL == fd) {
                printf("open fd error\n");
                return RET_ERROR;
            }
            ret = fread(resp, sizeof(char), sizeof(resp), fd);
            pclose(fd);
            if (!ret || strlen(resp) < 1) {
                printf("Unpredictiable error! resp data:%s\n", resp);
                return RET_ERROR;
            } else {
                *uid = atoi(resp);
                printf("Get valid user id:%d\n", *uid);
                break;
            }
        }
        if (*uid == 0) {
            printf("No active user! will use default value!\n");
            *uid = 1000;
        }
        // if variable username existed, will try to get username for further use.
        if (username == NULL) {
            printf("NULL pointer!\n");
        } else {
            snprintf(get_active_session_id, 64, "loginctl show-session %d -p Name --value", valid_session_id);
            fd = popen(get_active_session_id, RDONLY);
            if (NULL == fd) {
                printf("open fd error\n");
                return RET_ERROR;
            }
            ret = fread(resp, sizeof(char), sizeof(resp), fd);
            pclose(fd);
            if (!ret || strlen(resp) < 1) {
                printf("Unpredictiable error! resp data:%s\n", resp);
                return RET_ERROR;
            } else {
                // use strtok to cut and get the first part.
                strtok(resp, "\n");
                strncpy(username, resp, strlen(resp));
                printf("Get valid user name:%s\n", username);
            }
        }
        // if variable windowing system existed, will try to get windows system for further use.
        if (windowing_system == NULL) {
            printf("NULL pointer!\n");
        } else {
            snprintf(get_active_session_id, 64, "loginctl show-session %d -p Type --value", valid_session_id);
            fd = popen(get_active_session_id, RDONLY);
            if (NULL == fd) {
                printf("open fd error\n");
                return RET_ERROR;
            }
            ret = fread(resp, sizeof(char), sizeof(resp), fd);
            pclose(fd);
            if (!ret || strlen(resp) < 1) {
                printf("Unpredictiable error! resp data:%s\n", resp);
                return RET_ERROR;
            } else {
                // strncpy(username, resp, strlen(resp));
                if (strstr(resp, "wayland") != NULL) {
                    printf("Get valid windowing system:%s\n", resp);
                    *windowing_system = 0;
                } else if (strstr(resp, "X") != NULL) {
                    printf("Get valid windowing system:%s\n", resp);
                    *windowing_system = 1;
                } else {
                    printf("Unknown windowing system:%s, treat it as X11!\n", resp);
                    *windowing_system = 1;
                }
            }
        }
    }
    return RET_OK;
}
int fibocom_get_zenity_environment_variable(Progress *progress)
{
    printf("enter!\n");
    // check current windowing system and execute corresponding env init process.
    int  ret                       = RET_ERROR;
    int  uid                       = 0;
    char username[32]              = {0};
    int windowing_system           = -1;
    if (progress == NULL) {
        printf("NULL pointer!\n");
        return RET_ERROR;
    }
    // step1: get active user's UID!
    ret = find_active_user_id(&uid, username, &windowing_system);
    if (ret != RET_OK || uid < 1000 || strlen(username) < 1 || windowing_system < 0) {
        printf("can't get active user information!\n");
        return RET_ERROR;
    }
    // x11 will init xorg by default, but wayland won't init xorg by default!
    // but on fedora OS, there might be a thread called abrt-dump-journal-xorg -fxtD,
    // so we drop these code and use loginctl instead.
    if (windowing_system == 0) {
        printf("Current Windowing system: Wayland!");
        ret = init_env_for_wayland(progress, uid, username);
    }
    else if (windowing_system == 1) {
        printf("Current Windowing system: X11");
        ret = init_env_for_x11(progress, uid, username);
    }
    else {
        printf("unknown Windowing ststem!\n");
        return RET_ERROR;
    }
    if (ret != RET_OK) {
        printf("can't initialize env for windowing system!\n");
        return RET_ERROR;
    }
    // on Fedora OS, there might be error to show the string directly, but variable is correct.
    printf("progress bar env: \n%s\n", progress->environmentVariable);
    // for (int i = 0; i < 256; i++) {
    //    ROLLING_LOG_DEBUG("DEBUG: %d, %c", *((progress->environmentVariable) + i), *((progress->environmentVariable) + i));
    // }
    return RET_OK;
}
int fibocom_start_zenity(Progress *progress)
{
    printf("enter!\n");
    // env only work inside popen, aka a sub thread, parent thread won't accept these env.
    // so if a popen fd is closed, sub thread won't existed, env won't existed as well!
    snprintf(progress->progressCmd, 1024,
            "%s /usr/bin/zenity --progress --text=\"%s\" --percentage=%c --auto-close --no-cancel --width=600 --title=\"%s\"",
            progress->environmentVariable, progress->progressText, progress->progressSchedule[0], progress->progressTitle);
    // on Fedora OS, there might be error to show the string directly, but variable is correct.
    printf("Progress bar init string:\n%s\n", progress->progressCmd);
    // for (int i = 0; i < 256; i++) {
    //     ROLLING_LOG_DEBUG("DEBUG: %d, %c", *((progress->environmentVariable) + i), *((progress->environmentVariable) + i));
    // }
    progress->progressFd = popen(progress->progressCmd, "w");
    if(progress->progressFd == NULL)
        printf("fibocom_start_zenity error\n");
    sleep(2);
    return RET_OK;
}
int fibocom_set_zenity_title(Progress *progress, const char* title)
{
    printf("enter!\n");
    strcpy(progress->progressTitle, title);
    return RET_OK;
}
int fibocom_set_zenity_init_text(Progress *progress)
{
    printf("enter!\n");
    strcpy(progress->progressText, "<span font='13'>Downloading ...\\n\\n</span><span foreground='red' font='16'>Do not shut down or restart</span>");
    return RET_OK;
}
int fibocom_set_zenity_text(Progress *progress, const char *text)
{
    printf("enter!\n");
    char textTmp[256] = "#";
    strcat(textTmp, text);
    fibocom_remove_last_newline(textTmp);
    strcat(textTmp, "\\n\\n\n");
    strcpy(progress->progressText, textTmp);
    return RET_OK;
}
int fibocom_set_zenity_schedule(Progress *progress, int schedule)
{
    printf("enter!\n");
    char scheduleTemp[32] = {0};
    itoa(schedule, scheduleTemp, 10);
    strcpy(progress->progressSchedule, scheduleTemp);
    strcat(progress->progressSchedule, "\n");
    return RET_OK;
}
int fibocom_refresh_zenity(Progress *progress, const char *text, int schedule)
{
    printf("enter!\n");
    printf("progress->progressText = %s\n", progress->progressText);
    printf("progress->progressSchedule = %s\n", progress->progressSchedule);
    if(atoi(progress->progressSchedule) == 99)
        fwrite(progress->progressText, sizeof(char), strlen(progress->progressText), progress->progressFd);
    fwrite(progress->progressSchedule, sizeof(char), strlen(progress->progressSchedule), progress->progressFd);
    fflush(progress->progressFd);
    sleep(1);
    return RET_OK;
}
int fibocom_close_zenity(Progress *progress)
{
    printf("enter!\n");
    fclose(progress->progressFd);
    return RET_OK;
}
// end of zenity
// hptc_dialog
int fibocom_get_hptc_dialog_environment_variable(Progress *progress)
{
    char environmentVariable[] = "export DISPLAY=\":0\";export XDG_RUNTIME_DIR=\"/run/user/1000\";export XDG_CONFIG_DIRS=\"/etc/xdg:/usr/share/hptc-kwin-mgr/thinpro-settings\";";
    strncpy(progress->environmentVariable, environmentVariable, strlen(environmentVariable));
    return RET_OK;
}
int fibocom_start_hptc_dialog(Progress *progress)
{
    printf("enter!\n");
    sprintf(progress->progressCmd,
            "%s/usr/bin/hptc-dialog --id=%s --title=\"%s\" --timeout=30 --no-kbd-focus --width=%d --height=%d --hwidget=label --text=\"%s\" --vwidget=progressBar --style=Windows --maximum=100 --value=%s\n",
            progress->environmentVariable, progress->progressId, progress->progressTitle, progress->progressWidth, progress->progressHeight, progress->progressText, progress->progressSchedule);
    progress->progressFd = popen(progress->progressCmd, "w");
    if(progress->progressFd == NULL)
        printf("fibocom_start_hptc_dialog error\n");
    usleep(100);
    return RET_OK;
}
int fibocom_set_hptc_dialog_title(Progress *progress, const char* title)
{
    printf("enter!\n");
    //const char *title = "ModemUpgrade";
    strcpy(progress->progressTitle, title);
    return RET_OK;
}
int fibocom_set_hptc_dialog_init_text(Progress *progress)
{
    printf("enter!\n");
    strcpy(progress->progressText, "<p style=\\\"font-size: 15px\\\">Configuring mobile broadband device</p><p style=\\\"font-size: 15px;color: red;\\\">Do not shut down or restart ThinPro</p>");
    return RET_OK;
}
int fibocom_set_hptc_dialog_text(Progress *progress, const char* text)
{
    printf("enter!\n");
    strcpy(progress->progressText, text);
    return RET_OK;
}
int fibocom_set_hptc_dialog_schedule(Progress *progress, int schedule)
{
    printf("enter!\n");
    char scheduleTemp[32] = {0};
    itoa(schedule, scheduleTemp, 10);
    strcpy(progress->progressSchedule, scheduleTemp);
    // strcat(progress->progressSchedule, "\\n");
    return RET_OK;
}
int fibocom_close_hptc_dialog(Progress *progress)
{
    printf("enter!\n");
    sprintf(progress->progressCloseCmd,
            "%shptc-dialog --kill=fibocom",
            progress->environmentVariable);
    progress->progressCloseFd = popen(progress->progressCloseCmd,"w");
    if(progress->progressCloseFd == NULL) {
        printf("fibocom_close_hptc_dialog error\n");
        return RET_ERROR;
    }
    usleep(100);
    pclose(progress->progressCloseFd);
    return RET_OK;
}
int fibocom_refresh_hptc_dialog(Progress *progress, const char *text, int schedule)
{
    printf("enter!\n");
    fibocom_close_hptc_dialog(progress);
    if(schedule == 99)
        fibocom_set_hptc_dialog_text(progress, text);
    fibocom_set_hptc_dialog_schedule(progress, schedule);
    fibocom_start_hptc_dialog(progress);
    return RET_OK;
}
//end of hptc_dialog
Progress *CreateProgressImpl(enum CurrentDistibId hostType)
{
    printf("enter!\n");
    Progress *progressImpl = (Progress *)malloc(sizeof(Progress));
    if (progressImpl != NULL) {
        memset(progressImpl, 0, sizeof(Progress));
        switch (hostType) {
            case Ubuntu:
            case Fedora:
                progressImpl->fibocom_get_progress_environment_variable = fibocom_get_zenity_environment_variable;
                progressImpl->fibocom_start_progress = fibocom_start_zenity;
                progressImpl->fibocom_set_progress_title = fibocom_set_zenity_title;
                progressImpl->fibocom_set_progress_init_text = fibocom_set_zenity_init_text;
                progressImpl->fibocom_set_progress_text = fibocom_set_zenity_text;
                progressImpl->fibocom_set_progress_schedule = fibocom_set_zenity_schedule;
                progressImpl->fibocom_refresh_progress = fibocom_refresh_zenity;
                progressImpl->fibocom_close_progress = fibocom_close_zenity;
                break;
            case Thinpro:
                printf("Found Thinpro OS!\n");
                progressImpl->fibocom_get_progress_environment_variable = fibocom_get_hptc_dialog_environment_variable;
                progressImpl->fibocom_start_progress = fibocom_start_hptc_dialog;
                progressImpl->fibocom_set_progress_title = fibocom_set_hptc_dialog_title;
                progressImpl->fibocom_set_progress_init_text = fibocom_set_hptc_dialog_init_text;
                progressImpl->fibocom_set_progress_text = fibocom_set_hptc_dialog_text;
                progressImpl->fibocom_set_progress_schedule = fibocom_set_hptc_dialog_schedule;
                progressImpl->fibocom_refresh_progress = fibocom_refresh_hptc_dialog;
                progressImpl->fibocom_close_progress = fibocom_close_hptc_dialog;
                strcpy(progressImpl->progressId, "fibocom");
                break;
            case None:
            default:
                progressImpl->fibocom_get_progress_environment_variable = fibocom_get_zenity_environment_variable;
                progressImpl->fibocom_start_progress = fibocom_start_zenity;
                progressImpl->fibocom_set_progress_title = fibocom_set_zenity_title;
                progressImpl->fibocom_set_progress_init_text = fibocom_set_zenity_init_text;
                progressImpl->fibocom_set_progress_text = fibocom_set_zenity_text;
                progressImpl->fibocom_set_progress_schedule = fibocom_set_zenity_schedule;
                progressImpl->fibocom_refresh_progress = fibocom_refresh_zenity;
                progressImpl->fibocom_close_progress = fibocom_close_zenity;
            break;
        }
        progressImpl->progressHeight = 120;
        progressImpl->progressWidth = 600;
        progressImpl->progressFd = NULL;
        progressImpl->progressCloseFd = NULL;
        memset(progressImpl->progressTitle,       0, 64);
        memset(progressImpl->progressText,        0, 256);
        memset(progressImpl->progressSchedule,    0, 32);
        memset(progressImpl->environmentVariable, 0, 256);
        memset(progressImpl->progressCmd,         0, 1024);
        memset(progressImpl->progressCloseCmd,    0, 512);
    }
    return progressImpl;
}
void DestroyProgressImpl(Progress *self) {
    if (self != NULL) {
        free(self);
        self = NULL;
    }
}
int fibocom_get_current_distrib_id(enum CurrentDistibId *hostType)
{
    char get_current_distrib_id_cmd[] = "cat /etc/lsb-release | grep DISTRIB_ID | awk -F '=' '{print $2}'";
    FILE *get_current_distrib_id_fd = NULL;
    char distrib_id[64] = {0};
    int ret = 0;
    get_current_distrib_id_fd = popen(get_current_distrib_id_cmd, "r");
    if(get_current_distrib_id_fd == NULL)
        printf("get_current_distrib_id_fd error");
    ret = fread(distrib_id, sizeof(char), 64, get_current_distrib_id_fd);
    if(ret == RET_ERROR){
        printf("fread get_distrib_id error\n");
        if (find_fedora_file() == 0) {
            *hostType = Fedora;
        } else {
            *hostType = Ubuntu;
        }
        fclose(get_current_distrib_id_fd);
        return RET_OK;
    }
    fclose(get_current_distrib_id_fd);
    if(strstr(distrib_id, "Ubuntu")) {
        *hostType = Ubuntu;
    } else if(strstr(distrib_id, "ThinPro")) {
        *hostType = Thinpro;
    } else {
        //非空并且其他字段默认设置为Ubuntu
        *hostType = Ubuntu;
    }
    return RET_OK;
}