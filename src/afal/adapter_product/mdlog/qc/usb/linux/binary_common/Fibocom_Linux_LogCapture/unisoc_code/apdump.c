#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "misc.h"
#include "unisoc_log_main.h"

typedef struct smp_header
{
	unsigned int header;
	unsigned short smp_length;
	unsigned char  lcn;
	unsigned char type;
	unsigned short reserved;
	unsigned short check_sum;
	unsigned int diag_sn;
	unsigned short diag_length;
	unsigned char diag_type;
	unsigned char diag_subtype;
	unsigned int sub_cmd_type;
}smp_header_t;


extern bool islittleendian;                //展锐模组对log协议中的length 和SN 都是小端存储
extern const char *g_tftp_server_ip;       //NULL: 抓取ap dump 到本地 ; not NULL: 使用tftp抓取ap dump


bool  is_dump_finish = false;
int save_to_file(char *log_dir, unsigned char *buffer, int length)
{
    smp_header_t *smp_header = NULL;
    unsigned short memsize_short = 0;
    char *filename = NULL;
    static FILE *file; // 文件指针
    static int unisoc_logfile_dump = -1;
    int nwrites = 0;
    char filename_tmp[512] = {0};

    smp_header = (smp_header_t *)buffer;

    if(!islittleendian)
    {
        /* 大端模式，需要转换为小端 */
        //LogInfo("smp_header->sub_cmd_type is %u\n", smp_header->sub_cmd_type);
        smp_header->sub_cmd_type = swapInt(smp_header->sub_cmd_type);
        //LogInfo("big to little. smp_header->sub_cmd_type is %u\n", smp_header->sub_cmd_type);
    }

    switch (smp_header->sub_cmd_type)
    {
        case 0x00:
            /* begin: |header|memsize(8)|filename(128)| */
            
            filename = (char *)(buffer + sizeof(smp_header_t) + sizeof(uint64_t));
            LogInfo("filename is %-64s", filename);
            if(NULL != log_dir)
            {
                strcpy(filename_tmp,log_dir);
                strcat(filename_tmp, "/");
                strcat(filename_tmp, filename);
                //LogInfo("filename_tmp is %s", filename_tmp);
            }

            if (g_tftp_server_ip == NULL)
            {

                //打开文件，如果文件不存在则创建，如果文件已存在则覆盖原有内容
                file = fopen(filename_tmp, "a+");

                // 检查文件是否成功打开
                if (file == NULL)
                {
                    LogInfo("unable to create and open file %s\n", filename);
                    return 1;
                }
            }
            else
            {
                //创建文件
                unisoc_logfile_dump = ulog_logfile_create_fullname(0, filename, 0, 1);
            }
            break;
        case 0x01:
            /* data:  |header|mem| */

            if(!islittleendian)
            {
                /* 大端模式，需要转换为小端 */
                //LogInfo("smp_header->smp_length is %hu\n", smp_header->smp_length);
                smp_header->smp_length = swapShort(smp_header->smp_length);
                //LogInfo("big to little. smp_header->smp_length is %hu\n", smp_header->smp_length);
            }

            memsize_short = smp_header->smp_length-sizeof(smp_header_t) + sizeof(smp_header->header);
            //LogInfo("memsize_short is %lu\n", smp_header->smp_length-sizeof(smp_header_t) + sizeof(smp_header->header));
            
            if (g_tftp_server_ip == NULL)
            {
                // 将数据写入文件
                fwrite(buffer+ sizeof(smp_header_t), 1, memsize_short, file);
            }
            else
            {
                nwrites = ulog_logfile_save(unisoc_logfile_dump, buffer+ sizeof(smp_header_t), memsize_short);
                if (memsize_short != nwrites)
                {
                    LogInfo("memsize_short:%d  nwrites:%d\n",memsize_short,nwrites);
                    break;
                }
            }
            break;
        case 0x02:
            /*end:    |header|  */
            
            LogInfo("---file finish!\n");

            if (g_tftp_server_ip == NULL)
            {
                // 关闭文件
                fclose(file);
            }
            else
            {
                ulog_logfile_close(unisoc_logfile_dump);
            }
            break;

        case 0x03:
            /* sysdump all finish*/
            
            LogInfo("Ap sysdump all finish!\n");
            is_dump_finish = true;
            break;

        case 0x04:
            /*  time out , all finish */
            
            LogInfo("Timeout, ap sysdump all finish!\n");
            is_dump_finish = true;
            break;
        
        default:
            LogInfo("error cmd type %d\n", smp_header->sub_cmd_type);
            break;
    }

    return 0;
}