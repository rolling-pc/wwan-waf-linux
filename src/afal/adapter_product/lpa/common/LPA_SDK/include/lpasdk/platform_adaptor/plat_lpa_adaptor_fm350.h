/******************************************************************************
  Copyright (C), 2021, Shenzhen G&T Industrial Development Co., Ltd

  File:      plat_lpa_adaptor_fm350.h

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

#ifndef PLAT_LPA_ADAPTOR_FM350_H
#define PLAT_LPA_ADAPTOR_FM350_H


//#include <stdbool.h>
//#include <unistd.h>
//#include <pthread.h>
//#include <sys/time.h>
#include <string.h>
//#include <>
//#include <termios.h>

#ifdef __cplusplus
extern "C"
{
#endif 

#define BUFSIZE_MAX		(16*1024)

extern int pciot_lpa_tcflush(int fildes, int queue_selector);
extern int pciot_lpa_open(const char *pathname, int flags);	
extern int pciot_lpa_close(int fd);
extern size_t pciot_lpa_read(int fd, char *buf, size_t count);
extern size_t pciot_lpa_write(int fd, const char *buf, size_t count);
extern size_t pciot_lpa_write_qc151(int fd, const char *buf, size_t count);

#ifdef __cplusplus
}
#endif 

#endif


