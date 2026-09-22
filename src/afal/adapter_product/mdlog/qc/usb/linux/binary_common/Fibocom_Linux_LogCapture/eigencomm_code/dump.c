
#include "logdef.h"

#define MAX_RETRY_TIME 32
#define MAX_CMD_DATALEN	512
#define DUMP_CID 0xdc
#define N_DUMP_CID 0x23

#define GetDataCmd	0x20
#define GetInfoCmd	0x21
#define FinishCmd	0x25

enum RamDumpType {
	TRIGGER_ASSERT,
	RESPONSE_ASSERT,
	HANDSHAKE_READY,
	CHANGE_BAUD_RATE,
    SendReqGetInfo,
    SendRspGetInfo,
    SendReqGetCMD,
	SendFinishCMD,
	HANDSHAKE_READY2
};

typedef struct
{
	uint8_t Command;
	uint8_t Sequence;
	uint8_t CID;
	uint8_t NCID;
	uint16_t Status;
    uint16_t Length;
	uint8_t Data[MAX_CMD_DATALEN];
	uint32_t FCS;
}DumpRspWrap, *PtrDumpRspWrap;

typedef struct
{
	uint8_t Command;
	uint8_t Sequence;
	uint8_t CID;
	uint8_t NCID;
    uint32_t Length;
	uint8_t Data[MAX_CMD_DATALEN];
	uint32_t FCS;
}DumpReqWrap, *PtrDumpReqWrap;

typedef struct
{
	uint32_t ReadDataAddr;
	uint32_t ReadLen;
}ReadDataReqCell, *PtrReadDataReqCell;

ReadDataReqCell cell_dump_data[16];

#define MUTI_CELL_RAMDUMP_FILE_FLAG  (0x44554D50)
#define MUTI_DUMP_CELL_MAGIC    (0x43454C4C)


typedef struct
{
    uint32_t magic;
    uint8_t  filetype; //00:   01:32bit   02:64bit
    uint8_t  order; //not used
    uint16_t version; //from 1
    uint16_t cellcnt;
    uint16_t rsvd1;
    uint32_t rsvd2[4];
    uint16_t vt;
    uint16_t vtlen;

	/*
    RamDumpHeader()
    {
        magic = MUTI_CELL_RAMDUMP_FILE_FLAG;
        filetype = 0x01;
        version = 0x01;
        order = 0;
        cellcnt = 0;
        vt = 0;
        vtlen = 0;
    }
	*/
}RamDumpHeader;

typedef struct
{
    uint32_t magic;
    uint32_t addr;
    uint32_t len;
    uint32_t offset;
    uint32_t crc;
    uint32_t crcrsvd;
    uint32_t rsvd[4];
    
	/*
    DumpCell()
    {
        magic = MUTI_DUMP_CELL_MAGIC;
        addr = 0;
        len = 0;
        offset = 0;
        crc = crcrsvd = 0;
    }

    DumpCell(uint32_t naddr, uint32_t nlen)
    {
        magic = MUTI_DUMP_CELL_MAGIC;
        addr = naddr;
        len = nlen;
        offset = 0;
        crc = crcrsvd = 0;
    }
	*/
}DumpCell;


int dump_cell_cnt = 0;
int dump_uart_fd = 0;
int ramddump_file_fd = -1;

const uint16_t wCRCTalbeAbs[] =
{
    0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401, 0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400,
};

static uint16_t CRC16(uint16_t wCRC, uint8_t* pchMsg, uint16_t wDataLen)
{
    //uint16_t wCRC = 0xFFFF;
    uint16_t i;
    uint8_t chChar;
    for (i = 0; i < wDataLen; i++)
    {
        chChar = *pchMsg++;
        wCRC = wCRCTalbeAbs[(chChar ^ wCRC) & 15] ^ (wCRC >> 4);
        wCRC = wCRCTalbeAbs[((chChar >> 4) ^ wCRC) & 15] ^ (wCRC >> 4);
    }
    return wCRC;
}

void clear_uart_buffer()
{
	int ret = ioctl(dump_uart_fd, TCFLSH, TCIOFLUSH);
	if (ret != 0)
		trace_log ("tcflash error, %d: %s\n", errno, strerror(errno));
	clear_data();
}

int SendCommand(uint8_t *sbuf, size_t size, int crln)
{
    int ret = -1;
    int err_code = UE_TIMEOUT_ERR;

    if (crln == 1)
    {
        //Treatment of crln
        uint8_t sbuf_tmp[16] = {0};
        memmove(sbuf_tmp, sbuf, size);
        sbuf_tmp[size] = 0x0d;
        sbuf_tmp[size+1] = 0x0a;

        ret = write_uart_binary(dump_uart_fd, sbuf_tmp, size + 2);
        if (ret !=  -1)
        {
            return UE_OK;
        }
    }
    else
    {
        ret = write_uart_binary(dump_uart_fd, sbuf, size);
        if (ret != -1)
        {
            return UE_OK;
        }
    }

	struct termios2 term2;
	int rv = ioctl(dump_uart_fd, TCGETS2, &term2);

	if(rv != 0) {
		err_code = UE_DEVICE_NOT_CONNECT;
	}

    return err_code;
}

int SendRamDumpAck(int id, uint8_t *sbuf, size_t size)
{
    int CRLN = 0;
    int err_code;

    if (id == RESPONSE_ASSERT)
    {
        CRLN = 1;
        err_code = SendCommand(sbuf, size, CRLN);
    }
    else
    {
        err_code = SendCommand(sbuf, size, CRLN);
    }

    return err_code;
}

int WaitRamDumpAckAndData(int id, void *rbuf, int* psize, unsigned int timeout)
{
    int err_code = UE_TIMEOUT_ERR;
    int times = 0;
	int read_len = -1;
	struct timeval start, end;

	gettimeofday(&start, NULL);

    while (times < timeout)
    {
		
		//read_len = read_uart_binary(dump_uart_fd, rbuf, 65535);
		read_len = get_data(rbuf);
		
		if (read_len == 0)
		{
			//gettimeofday(&end, NULL);
			//times = (end.tv_sec * 1000 + end.tv_usec/1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);

			//trace_log("%s:read cost time %d\n", __func__, times);
			struct termios2 term2;
            int rv = ioctl(dump_uart_fd, TCGETS2, &term2);

            if(rv != 0) {
				trace_log("device closed!\n");
				gettimeofday(&end, NULL);
				times = (end.tv_sec * 1000 + end.tv_usec/1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);

				trace_log("wait read time %d\n", times);
                return UE_DEVICE_NOT_CONNECT;
            }
			//usleep(100);
			gettimeofday(&end, NULL);
			times = (end.tv_sec * 1000 + end.tv_usec/1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);

			continue;
		}
			
		if (SendReqGetCMD == id)
		{
			*psize = read_len;
			return UE_OK;
		}
		else if (HANDSHAKE_READY == id)
		{
			if (strstr((char*)rbuf, "DUMPDUMP"))
			{
				return UE_OK;
			}
		}
		else if (HANDSHAKE_READY2 == id)
		{
			if (strstr((char*)rbuf, "DUMP2"))
			{
				return UE_OK;
			}
		}
		else if (SendRspGetInfo == id)
		{
			if (!strstr((char*)rbuf, "DUMP")) //It is not the handshake data. If it is not found, it returns success
			{
				*psize = read_len;
				return UE_OK;
			}
		}

		gettimeofday(&end, NULL);
		times = (end.tv_sec * 1000 + end.tv_usec/1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);

    }
	trace_log("%s:timeout %d, len:%d\n", __func__, times, read_len);
    return err_code;
}

int ChangeBaudrate(int dumpmode, int usbmode)
{
	int ret;
    

    struct termios2 tty;

    ret = ioctl(dump_uart_fd, TCGETS2, &tty);
    if (ret != 0)
    {
    	trace_log("%s:get termios2 failed, error:%d\n", __func__, errno);
    	return ret;
    }
	 	// Set num. data bits
            tty.c_cflag     &=  ~CSIZE;			// CSIZE is a mask for the number of bits per character
            tty.c_cflag     |=  CS8;
            // Set parity
            tty.c_cflag     &=  ~PARENB; //NONE
            // Set num. stop bits
            tty.c_cflag     &=  ~CSTOPB;
            // Configure flow control
            tty.c_cflag &= ~CRTSCTS;

            tty.c_cflag     |=  CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
            //Set baud rates
            tty.c_cflag &= ~CBAUD;
            tty.c_cflag |= CBAUDEX;

			if (usbmode == 0) 
				{
					int bdr = 0;
					if (dumpmode > 0)
					{
						bdr = 921600;
					}
					else //hard coding for uart
					{
						bdr = 3000000;
					}

					trace_log("change baudrate:%d\n", bdr);
					tty.c_ispeed = bdr;
					tty.c_ospeed = bdr;
				}

            tty.c_oflag     =   0;              // No remapping, no delays
            tty.c_oflag     &=  ~OPOST;         // Make raw

            tty.c_cc[VTIME] = 0; 
            tty.c_cc[VMIN] = 0;
            //.c_iflag
            tty.c_iflag &= ~(IXON | IXOFF | IXANY);
            tty.c_iflag 	&= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|INPCK|IGNPAR|IUCLC|IMAXBEL|IUTF8);
            //LOCAL MODES (c_lflag)
            tty.c_lflag		&= ~ICANON;    // Turn off canonical input, which is suitable for pass-through
            tty.c_lflag &= ~(ECHO);
            tty.c_lflag		&= ~ECHOE;     // Turn off echo erase (echo erase only relevant if canonical input is active)
            //tty.c_lflag		&= ~ECHONL;    //
            tty.c_lflag		&= ~ISIG;      // Disables recognition of INTR (interrupt), QUIT and SUSP (suspend) characters
            
    /*
	// Set num. data bits
    tty.c_cflag     &=  ~CSIZE;			// CSIZE is a mask for the number of bits per character
    tty.c_cflag     |=  CS8;
    // Set parity
    tty.c_cflag     &=  ~PARENB; //NONE
    // Set num. stop bits
    tty.c_cflag     &=  ~CSTOPB;
    // Configure flow control
    tty.c_cflag &= ~CRTSCTS;

    tty.c_cflag     |=  CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
    //Set baud rates
    tty.c_cflag &= ~CBAUD;
    tty.c_cflag |= CBAUDEX;
    tty.c_ispeed = bdr;
    tty.c_ospeed = bdr;

    tty.c_oflag     =   0;              // No remapping, no delays
    tty.c_oflag     &=  ~OPOST;         // Make raw
	*/

	/*
    if (dumpmode > 0)
    {
		if (usbmode > 0)
			tty.c_cc[VTIME] = 0;
		else
			tty.c_cc[VTIME] = 1;
		tty.c_cc[VMIN] = 1;
    }
    else
    {
		tty.c_cc[VTIME] = 0;
		tty.c_cc[VMIN] = 1;
    }
	*/

	/*
	tty.c_cc[VTIME] = 1;
	tty.c_cc[VMIN] = 1;

    //.c_iflag
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag 	&= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
    //LOCAL MODES (c_lflag)
    tty.c_lflag		&= ~ICANON;    // Turn off canonical input, which is suitable for pass-through
    tty.c_lflag &= ~(ECHO);
    tty.c_lflag		&= ~ECHOE;     // Turn off echo erase (echo erase only relevant if canonical input is active)
    tty.c_lflag		&= ~ECHONL;    //
    tty.c_lflag		&= ~ISIG;      // Disables recognition of INTR (interrupt), QUIT and SUSP (suspend) characters
    */
	ret = ioctl(dump_uart_fd, TCSETS2, &tty);
    if (ret != 0)
    	trace_log("change baudrate error:%d", errno);
    /*
    struct termios2 tm;

    ret = ioctl(dump_uart_fd, TCGETS2, &tm);
	if (ret != 0)
	{
		trace_log("read back termios failed!\n");
		return ret;
	}
	else
	{
		trace_log("\nread baudrate ispeed:%d, ospeed:%d\n", tm.c_ispeed, tm.c_ospeed);
	}
	*/
    return ret;
}

int SyncStart()
{
	int errCode = UE_TIMEOUT_ERR;
	int nRetry = 0;

	uint8_t szRespAssertCmd[] = "okokok";

	for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
	{
		usleep(1000 * 10);
		errCode = SendRamDumpAck(RESPONSE_ASSERT, szRespAssertCmd, sizeof(szRespAssertCmd));
		if (errCode == UE_OK && nRetry > 2)
			break;
	}

	if (errCode != UE_OK)
	{
		trace_log("Send command RESPONSE_ASSERT ok failed");
		return errCode;
	}

	for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
	{
		if (ChangeBaudrate(1, 0) == 0)
			break;
		++nRetry;
		if (nRetry > MAX_RETRY_TIME)
		{
			trace_log("Trying change baudrate %d times failed!", nRetry);
			return UE_NOT_COMMAND_COMM_ERR;
		}
	}

	int nSendTime = 0;
	uint8_t messageBuf[65535] = { 0 };
	for (nRetry = 0; nRetry < MAX_RETRY_TIME / 4 && nSendTime < 6; ++nRetry)
	{
		int nTemp = 0;
		if ((errCode = WaitRamDumpAckAndData(HANDSHAKE_READY, messageBuf, &nTemp, 200)) != UE_OK)
		{
			if (errCode != UE_OK && errCode != UE_TIMEOUT_ERR)
			continue;
			/*
			if (errCode == UE_TIMEOUT_ERR)
				break;
			else
				continue;
			*/
		}

		if (errCode == UE_OK)
		{
			//usleep(500);
			uint8_t szHSAssertCmd[] = "DUMPDUMP";
			errCode = SendRamDumpAck(HANDSHAKE_READY, szHSAssertCmd, sizeof(szHSAssertCmd));
			if (errCode == UE_OK)
				nSendTime++;
		}

	}

	/*if (nSendTime <= 0)
	{
		trace_log("SendAndWaitHandShake failed");
		return UE_TIMEOUT_ERR;
	}*/

	return UE_OK;
}


int SyncStartUsb(void)
{
    int err_code = UE_TIMEOUT_ERR;
    uint8_t respAssertCmd[] = "okokok";
    uint8_t messageBuf[65535] = {0};
    int nRetry = 0;

	//usleep(5000 * 1000);
	clear_uart_buffer();

    for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
    {

        err_code = SendRamDumpAck(RESPONSE_ASSERT, respAssertCmd, 6);  //Either -1 or the number of bytes actually written to the module is returned
        if (err_code != UE_OK)
        {
            trace_log("%s SendRamDumpAck error: %s\n", __func__, strerror(errno));
            continue;
        }
	    else
	    {
	        trace_log("%s SendRamDumpAck success\n", __func__);
	    }


        int nTemp = 0;
 	    memset(messageBuf, 0, 65535);
        err_code = WaitRamDumpAckAndData(HANDSHAKE_READY, messageBuf, &nTemp, 2000);   //Either -1 or the number of bytes actually read from the module is returned
        if (err_code == UE_OK)
        {
            trace_log("%s WaitRamDumpAckAndData success\n", __func__);
            break;
        }
	    else
	    {
            trace_log("%s WaitRamDumpAckAndData fail\n", __func__);
	    }
    }

    if (nRetry >= MAX_RETRY_TIME && err_code != UE_OK)
    {
        return err_code;
    }

    usleep(400*1000);

    uint8_t hsAssertCmd[] = "DUMPDUMP";

    for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
    {
        err_code = SendRamDumpAck(HANDSHAKE_READY, hsAssertCmd, sizeof(hsAssertCmd));
        if (err_code != UE_OK)
        {
			trace_log("send dumpdump failed! retry it\n");
            continue;
        }

        int nTemp1 = 0;
        err_code = WaitRamDumpAckAndData(HANDSHAKE_READY, messageBuf, &nTemp1, 400);
        if (err_code == UE_OK)
        {
	        trace_log("%s WaitRamDumpAckAndData success -1\n", __func__);
            break;
        }
		else
			trace_log("dumpdump WaitRamDumpAckAndData failed\n");
    }

    if (nRetry >= MAX_RETRY_TIME && err_code != UE_OK)
    {
        return err_code;
    }

    return UE_OK;

}

int SendGetInfo()
{
	int nRetry = 0;
	uint8_t uSeq = 0;
	int errCode = UE_TIMEOUT_ERR;
	uint8_t messageBuf[65535] = { 0 };
	uint8_t message[65535] = { 0 };

	//send get info cmd
	DumpReqWrap dataReq;
	memset((void*)&dataReq, 0, sizeof(dataReq));
	dataReq.CID = DUMP_CID;
	dataReq.NCID = N_DUMP_CID;
	dataReq.Command = GetInfoCmd;
	dataReq.Sequence = uSeq++;
	dataReq.Length = 0;
	dataReq.FCS = 10;

	uint8_t data[256] = {0};
	int nCount = 0;

	nCount = sizeof(dataReq.Command) + sizeof(dataReq.CID) +
		sizeof(dataReq.Sequence) + sizeof(dataReq.NCID) + sizeof(dataReq.Length);

	const int nRspHeaderCount = sizeof(uint8_t) * 4 + sizeof(uint16_t) * 2;
	memcpy(data, (void*)&dataReq, nCount);

	if (dataReq.Length > 0)
	{
		memcpy((void*)&data[nCount], dataReq.Data, dataReq.Length);
		nCount += dataReq.Length;
	}

	dataReq.FCS = CRC16(0xFFFF, data, nCount);
	memcpy((void*)&data[nCount], (void*)&dataReq.FCS, sizeof(dataReq.FCS));

	nCount += sizeof(dataReq.FCS);

	ReadDataReqCell *readData = NULL;
	int bSuc = 0;
	DumpRspWrap* dataRsp = NULL;

	usleep(1000 * 100);
	clear_uart_buffer();

	for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
	{
		errCode = SendRamDumpAck(SendReqGetInfo, data, nCount);

		if (errCode == UE_DEVICE_NOT_CONNECT)
		{
			trace_log("maybe device is not connected!\n");
			return errCode;
		}
		if (errCode != UE_OK)
		{
			trace_log("send get info req failed! error : %d\n", errCode);
			continue;
		}

		dataRsp = NULL;
        int nPktIndex = 0;

		for (int ntry = 0; ntry < MAX_RETRY_TIME / 8; ++ntry)
		{
			//wait
			int nMsgDataLen = 0;
			errCode = WaitRamDumpAckAndData(SendRspGetInfo, messageBuf, &nMsgDataLen, 500);
			if (errCode != UE_OK)
			{
				continue;

			}

			memcpy(message + nPktIndex, messageBuf, nMsgDataLen);
			nPktIndex += nMsgDataLen;

			dataRsp = (DumpRspWrap*)message;

			if (dataRsp->Length > (nPktIndex - nRspHeaderCount - sizeof(dataRsp->FCS)))
				continue;
			else
				break;
		}

		if (errCode != UE_OK)
			continue;

		uint16_t crc = CRC16(0xFFFF, message, dataRsp->Length + nRspHeaderCount);
		uint32_t srcCrc = *(uint32_t*)(message + nRspHeaderCount + dataRsp->Length);
		if (crc == srcCrc)
		{
			bSuc = 1;
			break;
		}
	}

	if (!bSuc)
	{
		trace_log("Send command SendRspGetInfo failed!");
		return UE_NOT_COMMAND_COMM_ERR;
	}

	const int nCellSize = sizeof(ReadDataReqCell);
	for (int i = 0; i < dataRsp->Length / nCellSize; i++)
	{
		readData = (ReadDataReqCell*)(message + nRspHeaderCount + i * nCellSize);

        memmove(&cell_dump_data[i], readData, nCellSize);
        dump_cell_cnt++;
	}

	return UE_OK;
}


int SendGetInfoUsb(void)
{
    int nRetry = 0;
    uint8_t uSeq = 0;
    int err_code = UE_TIMEOUT_ERR;

    uint8_t messageBuf[65535] = { 0 };
	uint8_t message[65535] = { 0 };

    DumpReqWrap dataReq;
	memset(&dataReq, 0, sizeof(dataReq));
	dataReq.CID = DUMP_CID;
	dataReq.NCID = N_DUMP_CID;
	dataReq.Command = GetInfoCmd;
	dataReq.Sequence = uSeq++;
	dataReq.Length = 0;
	dataReq.FCS = 10;

    uint8_t data[256];
	int nCount = 0;

    nCount = sizeof(dataReq.Command) + sizeof(dataReq.CID) +
		sizeof(dataReq.Sequence) + sizeof(dataReq.NCID) + sizeof(dataReq.Length);

    const int nRspHeaderCount = sizeof(uint8_t) * 4 + sizeof(uint16_t) * 2;

    memcpy(data, (void*)&dataReq, nCount);

    if (dataReq.Length > 0)
	{
		memcpy((void*)&data[nCount], dataReq.Data, dataReq.Length);
		nCount += dataReq.Length;
	}

    dataReq.FCS = CRC16(0xFFFF, data, nCount);

    ReadDataReqCell *readData = NULL;
    int bSuc = 0;
    DumpRspWrap* dataRsp = NULL;

    for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
	{
		clear_uart_buffer();
        sleep(1);

        err_code = SendRamDumpAck(SendReqGetInfo, data, nCount);
		if (err_code == UE_DEVICE_NOT_CONNECT)
			return err_code;
        if (err_code != UE_OK)
            continue;

        err_code = SendRamDumpAck(SendReqGetInfo, (uint8_t*)&dataReq.FCS, sizeof(dataReq.FCS));
		if (err_code == UE_DEVICE_NOT_CONNECT)
			return err_code;
        if (err_code != UE_OK)
            continue;

        dataRsp = NULL;

        int nPktIndex = 0;
        int ntry;

		for (ntry = 0; ntry < MAX_RETRY_TIME / 6; ++ntry)
		{
			//wait
			int nMsgDataLen = 0;
			err_code = WaitRamDumpAckAndData(SendRspGetInfo, messageBuf, &nMsgDataLen, 500);

			if (err_code == UE_TIMEOUT_ERR)
                continue;
			else if ( err_code == UE_DEVICE_NOT_CONNECT)
				return err_code;

			memcpy(message + nPktIndex, messageBuf, nMsgDataLen);
			nPktIndex += nMsgDataLen;



			dataRsp = (DumpRspWrap*)message;

			if (dataRsp->Length > (int)(nPktIndex - nRspHeaderCount - sizeof(dataRsp->FCS)))
			{
				continue;
            }
			else
            {
				break;
            }
		}

		if (err_code != UE_OK)
            continue;

		uint16_t crc = CRC16(0xFFFF, message, dataRsp->Length + nRspHeaderCount);
		uint32_t srcCrc = *(uint32_t*)(message + nRspHeaderCount + dataRsp->Length);

		if (crc == srcCrc)
		{
			bSuc = 1;
			break;
		}
	}

    if (!bSuc)
	{
		return UE_NOT_COMMAND_COMM_ERR;
	}

    const int nCellSize = sizeof(ReadDataReqCell);
    int i;

    for (i = 0; i < dataRsp->Length / nCellSize; i++)
    {
        readData = (ReadDataReqCell*)(message + nRspHeaderCount + i * nCellSize);

		if (readData)
		{
            memmove(&cell_dump_data[i], readData, nCellSize);
            dump_cell_cnt++;
		}
    }

    return UE_OK;
}

char *get_time_name(int hasms)
{
    static char time_name[80];
    struct timeval _time;
    
	if ( hasms == 1)
	{
		if (gettimeofday(&_time, NULL) == 0)
		{
			struct tm* local = localtime(&_time.tv_sec);
			if (local != NULL)
			{
				snprintf(time_name, sizeof(time_name), "%04d%02d%02d_%02d%02d%02d.%03ld",
				local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
				local->tm_hour, local->tm_min, local->tm_sec, _time.tv_usec / 1000);
			}
			else
				snprintf(time_name, sizeof(time_name), "%s","0000");
		}
		else
			snprintf(time_name, sizeof(time_name), "%s","0000");
	}
    else
	{
		time_t ltime;
		struct tm *currtime;

		time(&ltime);
		currtime = localtime(&ltime);

		snprintf(time_name, sizeof(time_name), "%04d%02d%02d_%02d%02d%02d",
		(currtime->tm_year+1900), (currtime->tm_mon+1), currtime->tm_mday,
		currtime->tm_hour, currtime->tm_min, currtime->tm_sec);
	}


    return time_name;
}

int WriteData(uint8_t* pData, int nLen)
{
    int writeResult = write_uart_binary(ramddump_file_fd, pData , nLen);
    if ( writeResult == -1)
    {
        return -1;
    }

    return writeResult;
}

int GetDumpData(uint32_t nTotalData, uint32_t nReadAddr, int bFinish)
{
	uint32_t nReadLen = 0;
	int nReadTime = 0;
	int uSeq = 1;
	int nRetry = 0;
	int errCode = UE_TIMEOUT_ERR;
	uint8_t messageBuf[65535] = { 0 };
	uint8_t message[65535] = { 0 };

	DumpReqWrap dataReq;
	memset((void*)&dataReq, 0x0, sizeof(dataReq));
	dataReq.CID = DUMP_CID;
	dataReq.NCID = N_DUMP_CID;
	dataReq.Length = 0;
	dataReq.FCS = 10;

	uint8_t data[256];
	int nPktIndex = 0;
	ReadDataReqCell newReadData;
	newReadData.ReadLen = MAX_CMD_DATALEN;
	newReadData.ReadDataAddr = nReadAddr;

	int nCount = 0;

	nCount = sizeof(dataReq.Command) + sizeof(dataReq.CID) +
		sizeof(dataReq.Sequence) + sizeof(dataReq.NCID) + sizeof(dataReq.Length);

	const int nReqHeaderCount = nCount;
	const int nRspHeaderCount = sizeof(uint8_t) * 4 + sizeof(uint16_t) * 2;

	while (nReadLen < nTotalData && nTotalData > 0 && nReadTime < MAX_RETRY_TIME)
	{
		dataReq.Command = GetDataCmd;

		memcpy(dataReq.Data, (void*)&newReadData, sizeof(newReadData));

		dataReq.Length = sizeof(ReadDataReqCell)/*dataRsp->Length*/;
		dataReq.Sequence = uSeq; //失败重试不累加

		nCount = nReqHeaderCount;

		memcpy(data, (void*)&dataReq, nCount);

		if (dataReq.Length > 0)
		{
			memcpy((void*)&data[nCount], dataReq.Data, dataReq.Length);
			nCount += dataReq.Length;
		}
		dataReq.FCS = CRC16(0xFFFF, data, nCount);
		memcpy((void*)&data[nCount], (void*)&dataReq.FCS, sizeof(dataReq.FCS));

		nCount += sizeof(dataReq.FCS);

		clear_uart_buffer();

		for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
		{
			errCode = SendRamDumpAck(SendReqGetCMD, data, nCount);
			if (errCode == UE_OK)
				break;
		}

		if (errCode != UE_OK)
		{
			trace_log("Send command SendReqGetCMD failed, errcode:%d", errCode);
			return errCode;
		}

		DumpRspWrap* dataRsp = NULL;
		nPktIndex = 0;
		//for (nRetry = 0; nRetry < MAX_RETRY_TIME * 4; ++nRetry)
		nRetry = 0;
        uint32_t dwWaitTime = 0;
		while(nPktIndex < MAX_CMD_DATALEN + 12 && nRetry < MAX_CMD_DATALEN*2 )
		{
			int nMsgDataLen = 0;
			errCode = WaitRamDumpAckAndData(SendReqGetCMD, messageBuf, &nMsgDataLen,300);

            if ( errCode == UE_TIMEOUT_ERR)
                dwWaitTime += 300;
            if (errCode != UE_OK && dwWaitTime >= 1500)
            {
                trace_log("Wait timeout %d", dwWaitTime);
                break;
            }

			if (errCode != UE_OK)
			{
				nRetry++;
				trace_log("waiting get data error, %d\n", errCode);
				continue;
			}
			memcpy(message + nPktIndex, messageBuf, nMsgDataLen);
			nPktIndex += nMsgDataLen;

			dataRsp = (DumpRspWrap*)message;

			/*if (dataRsp->Length > nPktIndex - nRspHeaderCount)
			continue;
			else
			break;*/
		}

		if (errCode != UE_OK)
		{
			trace_log("Wait command SendReqGetCMD failed, ErrCode:%d", errCode);
			continue;
		}

        if (nRetry >= MAX_CMD_DATALEN * 2)
        {
            trace_log("Wait retry : %d", nRetry);
            return errCode;
        }

		if (errCode == UE_OK)
		{
			dataRsp = (DumpRspWrap*)message;
			uint8_t* pDumpData = (uint8_t*)(message + nRspHeaderCount);
			uint32_t dwDumpDataCRC = *(uint32_t*)(message + nRspHeaderCount + dataRsp->Length);
			uint16_t wNewCRC = CRC16(0xFFFF, message, dataRsp->Length + nRspHeaderCount);
			if (wNewCRC == (uint16_t)dwDumpDataCRC)
			{
				WriteData(pDumpData, dataRsp->Length);
				nReadLen += dataRsp->Length;
				uSeq++;
				newReadData.ReadDataAddr += dataRsp->Length;
				//TRACE(_T("dataRsq len:%d\n"), dataRsp->Length);
				//TRACE(_T("data addr:%x, nreadlen:%d, seq:%d\n"), newReadData.ReadDataAddr, nReadLen, uSeq - 1);

				if (nTotalData - nReadLen < MAX_CMD_DATALEN)
					newReadData.ReadLen = nTotalData - nReadLen;
				nReadTime = 0;
			}
			else
			{
				trace_log("crc error, datacrc: 0x%04x, newcrc: 0x%04x, seq:%d\n", (uint16_t)dwDumpDataCRC, wNewCRC, uSeq - 1);
				nReadTime++;
				continue;
			}
		}
	}


	if (nReadTime >= MAX_RETRY_TIME)
	{
		trace_log("GetDataCmd failed because retry time is over!");
		return UE_TIMEOUT_ERR;
	}

	if (bFinish)
	{
		//send finish cmd
		memset((void*)&dataReq, 0x0, sizeof(dataReq));
		dataReq.CID = DUMP_CID;
		dataReq.NCID = N_DUMP_CID;
		dataReq.Command = FinishCmd;
		dataReq.Sequence = uSeq++;
		dataReq.Length = 0;

		nCount = sizeof(dataReq.Command) + sizeof(dataReq.CID) +
			sizeof(dataReq.Sequence) + sizeof(dataReq.NCID) + sizeof(dataReq.Length);

		memcpy(data, (void*)&dataReq, nCount);

		dataReq.FCS = CRC16(0xFFFF, data, nCount);
		memcpy((void*)&data[nCount], (void*)&dataReq.FCS, sizeof(dataReq.FCS));

		nCount += sizeof(dataReq.FCS);

		for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
		{
			errCode = SendRamDumpAck(SendFinishCMD, data, nCount);
			if (errCode == UE_OK)
				break;
		}

		if (errCode != UE_OK)
		{
			trace_log("Send command SendFinishCMD failed!");
			return errCode;
		}
	}

	return UE_OK;
}


int GetDumpDataUsb(uint32_t nTotalData, uint32_t nReadAddr, int bFinish)
{
	int nReadLen = 0;
	int nReadTime = 0;
	int uSeq = 1;
	int nRetry = 0;
	int err_code = UE_TIMEOUT_ERR;;
	uint8_t messageBuf[65535] = { 0 };
	uint8_t message[65535] = { 0 };

	DumpReqWrap dataReq;
	memset((void*)&dataReq, 0x0, sizeof(dataReq));
	dataReq.CID = DUMP_CID;
	dataReq.NCID = N_DUMP_CID;
	dataReq.Length = 0;
	dataReq.FCS = 10;

	uint8_t data[256];
	int nPktIndex = 0;
    const uint32_t dwUsbMaxDataLen = MAX_CMD_DATALEN * 20;
	ReadDataReqCell newReadData = {0};
	newReadData.ReadLen = dwUsbMaxDataLen;
	newReadData.ReadDataAddr = nReadAddr;

	int nCount = 0;

	nCount = sizeof(dataReq.Command) + sizeof(dataReq.CID) +
		sizeof(dataReq.Sequence) + sizeof(dataReq.NCID) + sizeof(dataReq.Length);

	const int nReqHeaderCount = nCount;
	const int nRspHeaderCount = sizeof(uint8_t) * 4 + sizeof(uint16_t) * 2;

	usleep(200*1000);

	while (nReadLen < nTotalData && nTotalData > 0 && nReadTime < MAX_RETRY_TIME)
	{
		DumpRspWrap* dataRsp = NULL;
		nPktIndex = 0;
		nRetry = 0;

		dataReq.Command = GetDataCmd;

		dataReq.Length = sizeof(ReadDataReqCell)/*dataRsp->Length*/;
		dataReq.Sequence = uSeq; //Failed retries do not accumulate

		nCount = nReqHeaderCount;

		memcpy(data, (void*)&dataReq, nCount);

		clear_uart_buffer();

        for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
        {
            err_code = SendRamDumpAck(SendReqGetCMD, data, nCount);
            if (err_code == UE_OK)
                break;
        }

        if (dataReq.Length > 0)
        {
            memcpy(dataReq.Data, (void*)&newReadData, sizeof(newReadData));
            memcpy((void*)&data[nCount], dataReq.Data, dataReq.Length);
            nCount += dataReq.Length;
            for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
            {
                err_code = SendRamDumpAck(SendReqGetCMD, dataReq.Data, dataReq.Length);
                if (err_code == UE_OK)
                    break;
            }
        }

        dataReq.FCS = CRC16(0xFFFF, data, nCount);

        for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
        {
            err_code = SendRamDumpAck(SendReqGetCMD, (uint8_t*)&dataReq.FCS, sizeof(dataReq.FCS));
            if (err_code == UE_OK)
                break;
        }

        if (err_code != UE_OK)
        {
			trace_log("send get data cmd failed, 0x%02x%02x%02x%02x\n", 
				dataReq.Command, dataReq.Sequence, dataReq.CID, dataReq.NCID);
            continue;
        }
		
		while(nPktIndex < newReadData.ReadLen + 12 && nRetry < MAX_RETRY_TIME / 4)
		{
			int nMsgDataLen = 0;
			err_code = WaitRamDumpAckAndData(SendReqGetCMD, messageBuf, &nMsgDataLen, 500);
			if (err_code != UE_OK)
			{
				if (err_code == UE_DEVICE_NOT_CONNECT)
					return err_code;
				if ( err_code == UE_TIMEOUT_ERR)
					{
						trace_log("WaitRamDumpAckAndData time out,total: %d, getlen:%d, seq:%d\n", 
									nPktIndex, nMsgDataLen, uSeq - 1);
						usleep(1000);
					}

				nRetry++;
				continue;
			}
			memcpy(message + nPktIndex, messageBuf, nMsgDataLen);
			nPktIndex += nMsgDataLen;

			dataRsp = (DumpRspWrap*)message;

			//trace_log("dataRsp->Length:%d, total: %d , read: %d, retry:%d, seq:%d\n", 
			//		dataRsp->Length, nPktIndex, nMsgDataLen, nRetry, uSeq - 1);
			if (dataRsp->Length > newReadData.ReadLen)
				{
					trace_log("get dataRsp len: %d, newReadData.ReadLen : %d, seq: %d\n", dataRsp->Length, newReadData.ReadLen, uSeq - 1);
					usleep(1000);
					continue;
					break;
				}
		}

		if (err_code != UE_OK || nRetry >= MAX_RETRY_TIME / 4)
		{
			trace_log("not ue ok or out of retry(%d) \n", nRetry);
			continue;
		}

		if (err_code == UE_OK)
		{
			dataRsp = (DumpRspWrap*)message;
			if (dataReq.Command != dataRsp->Command || 
				dataReq.Sequence != dataRsp->Sequence ||
				dataReq.CID != dataRsp->CID ||
				dataReq.NCID != dataRsp->NCID)
				{
					trace_log("recv error data, send cmd : %02x%02x%02x%02x, recv cmd: %02x%02x%02x%02x, len:%d\n",
							dataReq.Command, dataReq.Sequence, dataReq.CID, dataReq.NCID,
							dataRsp->Command, dataRsp->Sequence, dataRsp->CID, dataRsp->NCID, nPktIndex);
					usleep(1000);
					continue;
				}



			uint8_t* pDumpData = (uint8_t*)(message + nRspHeaderCount);
			uint32_t dwDumpDataCRC = *(uint32_t*)(message + nRspHeaderCount + dataRsp->Length);
			uint16_t wNewCRC = CRC16(0xFFFF, message, dataRsp->Length + nRspHeaderCount);

			if (wNewCRC == (uint16_t)dwDumpDataCRC)
			{
				WriteData(pDumpData, dataRsp->Length);
				nReadLen += dataRsp->Length;
				uSeq++;
				newReadData.ReadDataAddr += dataRsp->Length;
				//trace_log("dataRsq len:%d\n", dataRsp->Length);
				//trace_log("data addr:%x, nreadlen:%d, seq:%d\n", newReadData.ReadDataAddr, nReadLen, uSeq - 1);

				if (nTotalData - nReadLen < dwUsbMaxDataLen)
					newReadData.ReadLen = nTotalData - nReadLen;
				nReadTime = 0;
			}
			else
			{
				trace_log("crc error, seq:%d, new CRC:0x%08x, real CRC:0x%08x data: %02x %02x %02x %02x\n", 
					uSeq - 1, wNewCRC, dwDumpDataCRC, message[0], message[1],message[2],message[3]);

				nReadTime++;
				continue;
			}
		}
	}

	if (nReadTime >= MAX_RETRY_TIME)
	{
		trace_log("retry read time :%d, failed!\n", nReadTime);
		return -1;
	}

    if ( bFinish)
    {
        //send finish cmd
        memset((void*)&dataReq, 0x0, sizeof(dataReq));
        dataReq.CID = DUMP_CID;
        dataReq.NCID = N_DUMP_CID;
        dataReq.Command = FinishCmd;
        dataReq.Sequence = uSeq++;
        dataReq.Length = 0;

        nCount = sizeof(dataReq.Command) + sizeof(dataReq.CID) +
            sizeof(dataReq.Sequence) + sizeof(dataReq.NCID) + sizeof(dataReq.Length);

        memcpy(data, (void*)&dataReq, nCount);

        for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
        {
            err_code = SendRamDumpAck(SendFinishCMD, data, nCount);
            if (err_code == UE_OK)
				break;
        }

		dataReq.FCS = CRC16(0xFFFF, data, nCount);
		memcpy((void*)&data[0], (void*)&dataReq.FCS, sizeof(dataReq.FCS));

		nCount = sizeof(dataReq.FCS);

		for (nRetry = 0; nRetry < MAX_RETRY_TIME; ++nRetry)
		{
			err_code = SendRamDumpAck(SendFinishCMD, data, nCount);
			if (err_code == UE_OK)
				break;
		}

        if (err_code != UE_OK)
        {
            return err_code;
        }
    }

	return UE_OK;
}

void PaddingDumpFile(int nLen)
{
    uint8_t szData[MAX_CMD_DATALEN] = {0};

    while (nLen > 0)
    {
        if (nLen > MAX_CMD_DATALEN)
        {
            WriteData(szData, MAX_CMD_DATALEN);
            nLen -= MAX_CMD_DATALEN;
        }
        else
        {
            WriteData(szData, nLen);
            nLen = 0;
        }
    }
}


int catch_dump_proc(/*int fd, uint8_t* pbuf, size_t size*/int usb_mode)
{
    int i;
    int err_code = UE_TIMEOUT_ERR;
    //int usb_mode = 1;

	/*
    int k;

    for(k=0; k < size; k++)
    {
        if ((size - k) > 4 && (
            (pbuf[k] == 0x22 && pbuf[k+1] == 0x04 && pbuf[k+2] == 0x02 && pbuf[k+3] == 0x00) ||
            (pbuf[k] == 0x22 && pbuf[k+1] == 0x04 && pbuf[k+2] == 0x01 && pbuf[k+3] == 0x80)
                )
            )
        {
            if (pbuf[k] == 0x22 && pbuf[k+1] == 0x04 && pbuf[k+2] == 0x01 && pbuf[k+3] == 0x80)
                usb_mode = 0;

            trace_log("Enter ramdump : usb_mode %d ...\n", usb_mode);
            break;
        }
    }

    if (k == size)
    {
        return 0;
    }
	*/



	trace_log("Enter ramdump : usb_mode %d ...\n", usb_mode);

    //dump_uart_fd = fd;
	dump_cell_cnt = 0;

    if (usb_mode)
        err_code = SyncStartUsb();
    else
        err_code = SyncStart();

    if (err_code != UE_OK)
    {
        trace_log("%s : SyncStart failed\n", __func__);
        return -1;
    }

    trace_log("handshake ok!\n");

    if (usb_mode)
        err_code = SendGetInfoUsb();
    else
        err_code = SendGetInfo();

    if (err_code != UE_OK)
    {
        trace_log("SendGetInfo failed, usbmode:%d\n", usb_mode);
        if (!usb_mode)
            ChangeBaudrate(0, 0);

        return -1;
    }

    trace_log("SendGetInfo ok!\n");

    char logFileName[100] = {0};
    char cure_dir_path[256] = {0};
    char dump_dir[262] = {0};

    snprintf(dump_dir, sizeof(dump_dir), "%s", "./ramdump");
    mkdir(dump_dir, 0755);

    snprintf(logFileName, sizeof(logFileName), "ramdump_%s.bin",get_time_name(0));
    snprintf(cure_dir_path,sizeof(cure_dir_path),"%.155s/%.80s",dump_dir,logFileName);
    trace_log("%s : ramdump file path:%s\n", __func__, cure_dir_path);

    ramddump_file_fd = open(cure_dir_path, O_CREAT | O_RDWR | O_TRUNC, 0444);
    if (ramddump_file_fd == -1)
    {
        trace_log("%s : open failed\n", __func__);
        return -1;
    }

	if (usb_mode)
		ChangeBaudrate(1, usb_mode);

	if (dump_cell_cnt == 3)
    {
		for (i = 0; i < dump_cell_cnt; i++)
		{
			trace_log("get data cell %d\n", i);
			if (usb_mode)
				err_code = GetDumpDataUsb(cell_dump_data[i].ReadLen, cell_dump_data[i].ReadDataAddr, i == dump_cell_cnt - 1);
			else
				err_code = GetDumpData(cell_dump_data[i].ReadLen, cell_dump_data[i].ReadDataAddr, i == dump_cell_cnt - 1);

			if (err_code != UE_OK)
			{
				if (ramddump_file_fd > 0)
				{
					close(ramddump_file_fd);
					ramddump_file_fd = -1;
				}
				remove(cure_dir_path);
				ChangeBaudrate(0, usb_mode);
				trace_log("GetDumpData failed!\n");
				return -1;
			}

			if ( i + 1 < dump_cell_cnt)
				PaddingDumpFile(cell_dump_data[i + 1].ReadDataAddr - (cell_dump_data[i].ReadLen + cell_dump_data[i].ReadDataAddr));
		}
	}
	else
	{
		RamDumpHeader rdHeader;
		memset((void*)&rdHeader, 0x0, sizeof(rdHeader));

		rdHeader.magic = MUTI_CELL_RAMDUMP_FILE_FLAG;
        rdHeader.filetype = 0x01;
        rdHeader.version = 0x01;
        rdHeader.cellcnt = dump_cell_cnt;
		WriteData((uint8_t*)&rdHeader, sizeof(rdHeader));

		DumpCell cell;
		memset((void*)&cell, 0x0, sizeof(cell));
		cell.magic = MUTI_DUMP_CELL_MAGIC;

		for (i = 0; i < dump_cell_cnt; i++)
		{
			cell.len = cell_dump_data[i].ReadLen;
			cell.addr = cell_dump_data[i].ReadDataAddr;
			WriteData((uint8_t*)&cell, sizeof(cell));
			
			trace_log("get data cell %d\n", i);
			if (usb_mode)
				err_code = GetDumpDataUsb(cell_dump_data[i].ReadLen, cell_dump_data[i].ReadDataAddr, i == dump_cell_cnt - 1);
			else
				err_code = GetDumpData(cell_dump_data[i].ReadLen, cell_dump_data[i].ReadDataAddr, i == dump_cell_cnt - 1);

			if (err_code != UE_OK)
			{
				if (ramddump_file_fd > 0)
				{
					close(ramddump_file_fd);
					ramddump_file_fd = -1;
				}
				remove(cure_dir_path);
				ChangeBaudrate(0, usb_mode);
				trace_log("GetDumpData failed!\n");
				return -1;
			}
		}

	}

    if (ramddump_file_fd > 0)
    {
        close(ramddump_file_fd);
        ramddump_file_fd = -1;
    }
	//if (usb_mode)
		ChangeBaudrate(0, usb_mode);

    trace_log("ramdump proc done ^_^\n");
    return 1;
}
