/******************************************************************************
  Copyright (C), 2021, Shenzhen G&T Industrial Development Co., Ltd

  File:      platform_lpa_adaptor.c

  Author:  fanming      
  Version: 1.0        
  Date:  2021.10
  
  Description:   

** History:
**Author (core ID)                Date          Number     Description of Changes
**-----------------------------------------------------------------------------
** 
** -----------------------------------------------------------------------------
******************************************************************************/
#include "lpasdk/platform_adaptor/platform_lpa_adaptor.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "lpasdk/core/lpa_log.h"
#include "lpa.hpp"

#define AT_EXT_LENGTH (10)
#define AT_CSIM_COMMAND_HEADER          "AT+CSIM="
#define CSIM_RESPONSE_PREFIX            "+CSIM:"

#define AT_CSIM_OPEN1 "AT+CSIM=10,\"0070000001\"\r"
#define AT_CSIM_OPEN1_RETUEN "+CSIM: 6,\"019000\"\rOK"
#define AT_CSIM_OPEN2 "AT+CSIM=44,\"01A4040010A0000005591010FFFFFFFF890000010000\"\r"
#define AT_CSIM_OPEN2_RETUEN "+CSIM: 240,\"6F748410A0000005591010FFFFFFFF8900000100A559734A06072A864886FC6B01600B06092A864886FC6B020202630906072A864886FC6B03650D060B2A864886FC6B05040200006618060A2B060104012A026E0103060A535433334A324D3002009F6E060077112701269F6501FFE00582030202029000\"\rOK"
#define AT_CSIM_OPEN2_ERROR_RETUEN "\r\nERROR\r\n\r\n"

#define AT_CSIM_CLOSE "AT+CSIM=10,\"0070800100\"\r"
#define AT_CSIM_CLOSE_RETUEN "+CSIM: 4,\"9000\"\rOK"

#define AT_CSIM_HEAD "AT+CSIM="
#define AT_CSIM_RETUEN_HEAD "+CSIM:"

#define AT_CHHO "AT+CCHO=\"A0000005591010FFFFFFFF8900000100\"\r"
#define AT_CCHC "AT+CCHC="

#define AT_CGLA_HEAD "AT+CGLA="
#define AT_CGLA_RETUEN_HEAD "+CGLA:"

typedef enum
{
    LPA_CMD_NORMAL = 0,
    LPA_CMD_OPEN = 1,
    LPA_CMD_CLOSE = 2,
}LPA_CMD_STATUS_enum;

char read_mipc_buf[BUFSIZE_MAX + 1];
static char tmp_mipc_buf[BUFSIZE_MAX + 1];
static int lpa_mipc_fd = 0;
static int lpa_cgla_port = 0;
static LPA_CMD_STATUS_enum lpa_cmd_status = LPA_CMD_NORMAL;

size_t pciot_lpa_csimtocgla(char* cgla_cmd, const char* csim_cmd);
size_t pciot_lpa_cglatocsim(char* csim_cmd, const char* cgla_cmd);

using namespace afal::lpa;

extern int SendAtOverMbimMessage(char *inputStr, char *OutputStr, int OutputStrSize);
extern int LpaGetChipPlatType(void);

int pciot_lpa_tcflush(int fildes, int queue_selector)
{
    memset(read_mipc_buf, 0, BUFSIZE_MAX + 1);
    return 0;
}

int pciot_lpa_open(const char* pathname, int flags)
{
    if (lpa_mipc_fd > 0)
    {
        return -1;
    }

    lpa_mipc_fd = 1;
    return lpa_mipc_fd;
}

int pciot_lpa_close(int fd)
{
    lpa_mipc_fd = 0;
    return 0;
}

size_t pciot_lpa_read(int fd, char* buf, size_t count)
{
    size_t count_real = strlen(read_mipc_buf);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Read count = %d, count_real = %d\n", count, count_real);
    strncpy(buf, read_mipc_buf, BUFSIZE_MAX);
    memset(read_mipc_buf, 0, BUFSIZE_MAX + 1);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "MIPC Read :%s\n", buf);
    return count_real;
}

size_t pciot_lpa_write_default(int fd, const char *buf, size_t count)
{
    size_t out_len = 0;
    int ret = SendAtOverMbimMessage((char *)buf, read_mipc_buf, sizeof(read_mipc_buf));
    if (ret < 0)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [error][%s:%d]Send AT Cmd fail \n", __FUNCTION__, __LINE__);
        return 0;
    }

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [info]csim_rsp:%s\n", read_mipc_buf);
    return count;
}

size_t pciot_lpa_write_mtk(int fd, const char* buf, size_t count)
{
    char* t_cgla_cmd = NULL;
    unsigned short t_cgla_rsp_len = 0;
    char* t_csim_rsp = NULL;
    char* t_csim_rsp_ptr = NULL;
    char* t_cgla_rsp_ptr = NULL;

    if (0 == strcmp(buf, AT_CSIM_OPEN1))
    {
        strncpy(read_mipc_buf, AT_CSIM_OPEN1_RETUEN, strlen(AT_CSIM_OPEN1_RETUEN));
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "OPEN1 read_mipc_buf:%s\n", read_mipc_buf);
        return count;
    }

    t_cgla_cmd = (char*)malloc(count + AT_EXT_LENGTH);
    if (t_cgla_cmd == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [error][%s:%d]-t_cgla_cmd malloc failed \n", __FUNCTION__, __LINE__);
        return 0;
    }
    memset(t_cgla_cmd, 0, (count + AT_EXT_LENGTH));

    char* t_cgla_rsp = (char*)malloc(BUFSIZE_MAX + 1);
    if (t_cgla_rsp == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [error]t_csim_rsp malloc failed \n");
        free(t_cgla_cmd);
        t_cgla_cmd = NULL;
        return 0;
    }
    memset(t_cgla_rsp, 0, BUFSIZE_MAX + 1);

    pciot_lpa_csimtocgla(t_cgla_cmd, buf);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [info]-csim2cgla csim cmd is %s \n", buf);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [info]-csim2cgla csim cmd len=%d \n", strlen(buf));
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [info]-csim2cgla cgla cmd is %s \n", t_cgla_cmd);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [info]-csim2cgla cgla cmd len=%d \n", strlen(t_cgla_cmd));

    int ret = SendAtOverMbimMessage(t_cgla_cmd, t_cgla_rsp, BUFSIZE_MAX + 1);
    if (ret < 0)
    {
        free(t_cgla_cmd);
        t_cgla_cmd = NULL;
        free(t_cgla_rsp);
        t_cgla_rsp = NULL;
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [error][%s:%d]Send AT Cmd fail \n", __FUNCTION__, __LINE__);
        return 0;
    }
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [info]t_cgla_rsp:%s\n", t_cgla_rsp);

    free(t_cgla_cmd);
    t_cgla_cmd = NULL;

    t_csim_rsp = (char*)malloc(BUFSIZE_MAX + 1);
    if (t_csim_rsp == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [error]t_csim_rsp malloc failed \n");
        free(t_cgla_rsp);
        return 0;
    }
    memset(t_csim_rsp, 0, BUFSIZE_MAX + 1);

    pciot_lpa_cglatocsim(t_csim_rsp, t_cgla_rsp);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPA LIB]: [info]t_csim_rsp:%s\n", t_csim_rsp);

    memset(read_mipc_buf, 0, sizeof(read_mipc_buf));
    strncpy(read_mipc_buf, t_csim_rsp, strlen(t_csim_rsp));
    free(t_csim_rsp);
    free(t_cgla_rsp);
    t_csim_rsp = NULL;
    t_cgla_rsp = NULL;
    return count;
}

size_t pciot_lpa_csimtocgla(char* cgla_cmd, const char* csim_cmd)
{
    char* ptr_csim_cmd = NULL;
    size_t cgla_cmd_loc = 0;

    if (0 == strcmp(csim_cmd, AT_CSIM_OPEN2))
    {
        strncpy(cgla_cmd, AT_CHHO, strlen(AT_CHHO));
        lpa_cmd_status = LPA_CMD_OPEN;
    }
    else if (0 == strcmp(csim_cmd, AT_CSIM_CLOSE))
    {
        strncpy(cgla_cmd, AT_CCHC, strlen(AT_CCHC));
        cgla_cmd_loc = strlen(cgla_cmd);
        cgla_cmd[cgla_cmd_loc] = lpa_cgla_port;
        cgla_cmd[cgla_cmd_loc + 1] = '\r';
        lpa_cmd_status = LPA_CMD_CLOSE;
    }
    else
    {
        strcat(cgla_cmd, AT_CGLA_HEAD);
        ptr_csim_cmd = (char *)csim_cmd + strlen(AT_CSIM_HEAD);
        cgla_cmd_loc = strlen(cgla_cmd);
        cgla_cmd[cgla_cmd_loc] = lpa_cgla_port;
        cgla_cmd[cgla_cmd_loc + 1] = ',';
        strncat(cgla_cmd, ptr_csim_cmd, strlen(csim_cmd) - strlen(AT_CSIM_HEAD));
        lpa_cmd_status = LPA_CMD_NORMAL;
    }
    return 0;
}

size_t pciot_lpa_cglatocsim(char* csim_cmd, const char* cgla_cmd)
{
    char* ptr_cgla_cmd = NULL;
    if (LPA_CMD_OPEN == lpa_cmd_status)
    {
        if (NULL == strstr(cgla_cmd, "OK"))
        {
            //lpa_cgla_port = '1';
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "open channel cmd fail:%s\n", cgla_cmd);
            strncpy(csim_cmd, AT_CSIM_OPEN2_ERROR_RETUEN, strlen(AT_CSIM_OPEN2_ERROR_RETUEN));
            lpa_cmd_status = LPA_CMD_NORMAL;
        }
        else
        {
            const char* tmp = strstr(cgla_cmd, AT_CHHO);
            if (tmp)
            {
                tmp += strlen(AT_CHHO);
                while (*tmp > '9' || *tmp < '0')
                {
                    tmp++;
                }
                std::string portStr(tmp, strstr(tmp, "\r") - tmp);
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "portStr:%s\n", portStr.c_str());
                lpa_cgla_port = atoi(portStr.c_str());//tmp[0];
                lpa_cgla_port += '0';
            }
            else
            {
                std::string portStr(cgla_cmd, strstr(cgla_cmd, "\r") - cgla_cmd);
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "portStr:%s\n", portStr.c_str());
                lpa_cgla_port = atoi(portStr.c_str());//tmp[0];
                lpa_cgla_port += '0';
                //lpa_cgla_port = cgla_cmd[0];
            }

            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpa_cgla_port:%d\n", lpa_cgla_port);
            strncpy(csim_cmd, AT_CSIM_OPEN2_RETUEN, strlen(AT_CSIM_OPEN2_RETUEN));
            lpa_cmd_status = LPA_CMD_NORMAL;
        }
    }
    else if (LPA_CMD_CLOSE == lpa_cmd_status)
    {
        strncpy(csim_cmd, AT_CSIM_CLOSE_RETUEN, strlen(AT_CSIM_CLOSE_RETUEN));
        lpa_cmd_status = LPA_CMD_NORMAL;
        lpa_cgla_port = 0;
    }
    else
    {
        strcat(csim_cmd, AT_CSIM_RETUEN_HEAD);
        const char* tmp = strstr(cgla_cmd, AT_CGLA_RETUEN_HEAD);
        const char* preTmp = tmp;
        while (tmp)
        {
            preTmp = tmp;
            tmp = strstr(preTmp + strlen(AT_CGLA_RETUEN_HEAD), AT_CGLA_RETUEN_HEAD);
        }

        if (preTmp)
        {
            ptr_cgla_cmd = (char*)preTmp + strlen(AT_CGLA_RETUEN_HEAD);
            strncat(csim_cmd, ptr_cgla_cmd, strlen(preTmp) - strlen(AT_CGLA_RETUEN_HEAD));
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "cgla response not find:%s\n", AT_CGLA_RETUEN_HEAD);
        }
    }
    return 0;
}

size_t pciot_lpa_write(int fd, const char* buf, size_t count)
{
    int platType = LpaGetChipPlatType();

    if (PLAT_TYPE_MTK == platType)
    {
        return pciot_lpa_write_mtk(fd, buf, count);
    }
    else if (PLAT_TYPE_QC_151 == platType)
    {
        return pciot_lpa_write_qc151(fd, buf, count);
    }
    else if (PLAT_TYPE_QC_135R == platType)
    {
        return pciot_lpa_write_qc151(fd, buf, count);
    }
    else
    {
        return pciot_lpa_write_default(fd, buf, count);
    }
}
