/******************************************************************************
  Copyright (C), 2024, Shenzhen G&T Industrial Development Co., Ltd

  File:      plat_lpa_adaptor_qc151.cpp

  Author:    
  Version:   1.0        
  Date:      2024
  
  Description:
    QC151 platform adaptor for LPA CGLA mode, handling multi-digit
    channel formatting and CLA de-channeling specific to this platform.

** History:
**-----------------------------------------------------------------------------
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

#define AT_CSIM_OPEN1_RETUEN "+CSIM: 6,\"019000\"\rOK"
#define AT_CSIM_OPEN2_RETUEN "+CSIM: 240,\"6F748410A0000005591010FFFFFFFF8900000100A559734A06072A864886FC6B01600B06092A864886FC6B020202630906072A864886FC6B03650D060B2A864886FC6B05040200006618060A2B060104012A026E0103060A535433334A324D3002009F6E060077112701269F6501FFE00582030202029000\"\rOK"
#define AT_CSIM_OPEN2_ERROR_RETUEN "\r\nERROR\r\n\r\n"

#define AT_CSIM_CLOSE_RETUEN "+CSIM: 4,\"9000\"\rOK"

#define AT_CSIM_HEAD "AT+CSIM="
#define AT_CSIM_RETUEN_HEAD "+CSIM:"

#define AT_CHHO "AT+CCHO=\"A0000005591010FFFFFFFF8900000100\"\r"
#define AT_CCHO_RETUEN_HEAD "+CCHO:"
#define AT_CCHC "AT+CCHC="

#define AT_CGLA_HEAD "AT+CGLA="
#define AT_CGLA_RETUEN_HEAD "+CGLA:"

typedef enum
{
    LPA_CMD_NORMAL = 0,
    LPA_CMD_OPEN = 1,
    LPA_CMD_CLOSE = 2,
}LPA_CMD_STATUS_enum;

// Per-session channel state for QC151 CGLA mode.
static int qc151_lpa_cgla_port = 0;
static LPA_CMD_STATUS_enum qc151_lpa_cmd_status = LPA_CMD_NORMAL;

static size_t qc151_csimtocgla(char* cgla_cmd, const char* csim_cmd);
static size_t qc151_cglatocsim(char* csim_cmd, const char* cgla_cmd);

extern char read_mipc_buf[BUFSIZE_MAX + 1];
extern int SendAtOverMbimMessage(char *inputStr, char *OutputStr, int OutputStrSize);

// Dispatched for PLAT_TYPE_QC_151 only. Formats multi-digit channel numbers
// as decimal strings and strips logical channel bits from interindustry APDU CLA bytes.
size_t pciot_lpa_write_qc151(int fd, const char* buf, size_t count)
{
    char* t_cgla_cmd = NULL;
    char* t_csim_rsp = NULL;

    if (0 == strcmp(buf, "AT+CSIM=10,\"0070000001\"\r"))
    {
        strncpy(read_mipc_buf, AT_CSIM_OPEN1_RETUEN,
                strlen(AT_CSIM_OPEN1_RETUEN));
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                         "QC151 OPEN1 read_mipc_buf:%s\n", read_mipc_buf);
        return count;
    }

    t_cgla_cmd = (char*)malloc(count + AT_EXT_LENGTH);
    if (t_cgla_cmd == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                         "[LPA LIB]: [error][%s:%d]-t_cgla_cmd malloc failed \n",
                         __FUNCTION__, __LINE__);
        return 0;
    }
    memset(t_cgla_cmd, 0, (count + AT_EXT_LENGTH));

    char* t_cgla_rsp = (char*)malloc(BUFSIZE_MAX + 1);
    if (t_cgla_rsp == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                         "[LPA LIB]: [error]t_cgla_rsp malloc failed \n");
        free(t_cgla_cmd);
        t_cgla_cmd = NULL;
        return 0;
    }
    memset(t_cgla_rsp, 0, BUFSIZE_MAX + 1);

    qc151_csimtocgla(t_cgla_cmd, buf);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                     "[LPA LIB]: [info]-csim2cgla csim cmd is %s \n", buf);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                     "[LPA LIB]: [info]-csim2cgla csim cmd len=%d \n",
                     strlen(buf));
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                     "[LPA LIB]: [info]-csim2cgla cgla cmd is %s \n",
                     t_cgla_cmd);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                     "[LPA LIB]: [info]-csim2cgla cgla cmd len=%d \n",
                     strlen(t_cgla_cmd));

    int ret = SendAtOverMbimMessage(t_cgla_cmd, t_cgla_rsp,
                                    BUFSIZE_MAX + 1);
    if (ret < 0)
    {
        free(t_cgla_cmd);
        t_cgla_cmd = NULL;
        free(t_cgla_rsp);
        t_cgla_rsp = NULL;
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                         "[LPA LIB]: [error][%s:%d]Send AT Cmd fail \n",
                         __FUNCTION__, __LINE__);
        return 0;
    }
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                     "[LPA LIB]: [info]t_cgla_rsp:%s\n", t_cgla_rsp);

    free(t_cgla_cmd);
    t_cgla_cmd = NULL;

    t_csim_rsp = (char*)malloc(BUFSIZE_MAX + 1);
    if (t_csim_rsp == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                         "[LPA LIB]: [error]t_csim_rsp malloc failed \n");
        free(t_cgla_rsp);
        return 0;
    }
    memset(t_csim_rsp, 0, BUFSIZE_MAX + 1);

    qc151_cglatocsim(t_csim_rsp, t_cgla_rsp);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                     "[LPA LIB]: [info]t_csim_rsp:%s\n", t_csim_rsp);

    memset(read_mipc_buf, 0, sizeof(read_mipc_buf));
    strncpy(read_mipc_buf, t_csim_rsp, strlen(t_csim_rsp));
    free(t_csim_rsp);
    free(t_cgla_rsp);
    t_csim_rsp = NULL;
    t_cgla_rsp = NULL;
    return count;
}

// Convert AT+CSIM to AT+CGLA with multi-digit channel support.
// Strips logical channel bits from interindustry-class CLA bytes to avoid double selection.
static size_t qc151_csimtocgla(char* cgla_cmd, const char* csim_cmd)
{
    char* ptr_csim_cmd = NULL;
    size_t cgla_cmd_loc = 0;

    if (0 == strcmp(csim_cmd, "AT+CSIM=44,\"01A4040010A0000005591010FFFFFFFF890000010000\"\r"))
    {
        strncpy(cgla_cmd, AT_CHHO, strlen(AT_CHHO));
        qc151_lpa_cmd_status = LPA_CMD_OPEN;
    }
    else if (0 == strcmp(csim_cmd, "AT+CSIM=10,\"0070800100\"\r"))
    {
        strncpy(cgla_cmd, AT_CCHC, strlen(AT_CCHC));
        cgla_cmd_loc = strlen(cgla_cmd);
        cgla_cmd_loc += sprintf(cgla_cmd + cgla_cmd_loc, "%d",
                                qc151_lpa_cgla_port);
        cgla_cmd[cgla_cmd_loc] = '\r';
        qc151_lpa_cmd_status = LPA_CMD_CLOSE;
    }
    else
    {
        strcat(cgla_cmd, AT_CGLA_HEAD);
        ptr_csim_cmd = (char *)csim_cmd + strlen(AT_CSIM_HEAD);
        cgla_cmd_loc = strlen(cgla_cmd);
        sprintf(cgla_cmd + cgla_cmd_loc, "%d,", qc151_lpa_cgla_port);
        strncat(cgla_cmd, ptr_csim_cmd,
                strlen(csim_cmd) - strlen(AT_CSIM_HEAD));
        qc151_lpa_cmd_status = LPA_CMD_NORMAL;

        // Strip logical channel encoding from interindustry-class CLA bytes.
        char* apdu_ptr = strchr(cgla_cmd, '"');
        if (apdu_ptr != NULL && *(apdu_ptr + 1) != '\0'
            && *(apdu_ptr + 2) != '\0')
        {
            if (*(apdu_ptr + 1) == '0')
            {
                char lo = *(apdu_ptr + 2);
                int lo_val = (lo >= '0' && lo <= '9') ? (lo - '0')
                             : (lo >= 'A' && lo <= 'F') ? (lo - 'A' + 10)
                             : (lo >= 'a' && lo <= 'f') ? (lo - 'a' + 10) : -1;
                if (lo_val >= 0)
                {
                    lo_val &= 0x0C;
                    *(apdu_ptr + 1) = '0';
                    *(apdu_ptr + 2) = (lo_val < 10) ? ('0' + lo_val)
                                                    : ('A' + lo_val - 10);
                }
            }
        }
    }
    return 0;
}

// Convert AT+CGLA response back to AT+CSIM format, supporting multi-digit channel ports.
static size_t qc151_cglatocsim(char* csim_cmd, const char* cgla_cmd)
{
    char* ptr_cgla_cmd = NULL;
    if (LPA_CMD_OPEN == qc151_lpa_cmd_status)
    {
        if (NULL == strstr(cgla_cmd, "OK"))
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                             "QC151 open channel cmd fail:%s\n", cgla_cmd);
            strncpy(csim_cmd, AT_CSIM_OPEN2_ERROR_RETUEN,
                    strlen(AT_CSIM_OPEN2_ERROR_RETUEN));
            qc151_lpa_cmd_status = LPA_CMD_NORMAL;
        }
        else
        {
            int parsed_port = -1;
            const char* p = strstr(cgla_cmd, AT_CCHO_RETUEN_HEAD);
            if (p != NULL)
            {
                p += strlen(AT_CCHO_RETUEN_HEAD);
            }
            else
            {
                p = strstr(cgla_cmd, AT_CHHO);
                if (p != NULL)
                {
                    p += strlen(AT_CHHO);
                }
            }
            while (p != NULL && *p != '\0' && (*p > '9' || *p < '0'))
            {
                p++;
            }
            if (p != NULL && *p != '\0')
            {
                parsed_port = atoi(p);
            }

            if (parsed_port < 0)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                                 "QC151 open channel port parse fail, rsp:%s\n",
                                 cgla_cmd);
                strncpy(csim_cmd, AT_CSIM_OPEN2_ERROR_RETUEN,
                        strlen(AT_CSIM_OPEN2_ERROR_RETUEN));
                qc151_lpa_cmd_status = LPA_CMD_NORMAL;
            }
            else
            {
                if (parsed_port == 0)
                {
                    // 0 may be a legal session id on some modems, keep the
                    // legacy behaviour instead of failing the open request.
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                                     "QC151 open channel parsed port 0, keep legacy behaviour, rsp:%s\n",
                                     cgla_cmd);
                }
                qc151_lpa_cgla_port = parsed_port;
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                                 "QC151 lpa_cgla_port:%d\n",
                                 qc151_lpa_cgla_port);
                strncpy(csim_cmd, AT_CSIM_OPEN2_RETUEN,
                        strlen(AT_CSIM_OPEN2_RETUEN));
                qc151_lpa_cmd_status = LPA_CMD_NORMAL;
            }
        }
    }
    else if (LPA_CMD_CLOSE == qc151_lpa_cmd_status)
    {
        // Log a failed CCHC so a leaked channel is visible instead of being
        // silently reported as a successful close to the LPA library.
        if (strstr(cgla_cmd, "OK") == NULL)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                             "QC151 close channel %d failed, rsp:%s\n",
                             qc151_lpa_cgla_port, cgla_cmd);
        }
        strncpy(csim_cmd, AT_CSIM_CLOSE_RETUEN,
                strlen(AT_CSIM_CLOSE_RETUEN));
        qc151_lpa_cmd_status = LPA_CMD_NORMAL;
        qc151_lpa_cgla_port = 0;
    }
    else
    {
        strcat(csim_cmd, AT_CSIM_RETUEN_HEAD);
        const char* tmp = strstr(cgla_cmd, AT_CGLA_RETUEN_HEAD);
        const char* preTmp = tmp;
        while (tmp)
        {
            preTmp = tmp;
            tmp = strstr(preTmp + strlen(AT_CGLA_RETUEN_HEAD),
                         AT_CGLA_RETUEN_HEAD);
        }

        if (preTmp)
        {
            ptr_cgla_cmd = (char*)preTmp
                           + strlen(AT_CGLA_RETUEN_HEAD);
            strncat(csim_cmd, ptr_cgla_cmd,
                    strlen(preTmp) - strlen(AT_CGLA_RETUEN_HEAD));
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                             "QC151 cgla response not find:%s\n",
                             AT_CGLA_RETUEN_HEAD);
        }
    }
    return 0;
}
