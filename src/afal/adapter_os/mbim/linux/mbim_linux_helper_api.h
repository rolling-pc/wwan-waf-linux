#ifndef MBIM_LINUX_HELPER_API_H
#define MBIM_LINUX_HELPER_API_H

#include "libmbim_common_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

int AfalMbimLinuxSimQuerySync(char *portname, _SimData *OutputData, int timeout);
int AfalMbimLinuxNetworkQuerySync(char *portname, _NetworkData *OutputData, int timeout);
int AfalMbimLinuxRadioQuerySync(char *portname, _RadioData *OutputData, int timeout);
int AfalMbimLinuxSlotQuerySync(char *portname, _SlotData *OutputData, int timeout);
int AfalMbimLinuxTraceLogQuerySync(char *portname, char *InputData, int *OutputData, int timeout);
//int AfalMbimLinuxAtOverMbimSetSync(char *portname, char *input_str, char *output_str, int timeout);
int AfalMbimLinuxAtOverMbimSetSync(char *portname, char *input_str, char *output_str, int timeout);
int AfalMbimLinuxSlotSetSync(char *portname, _SlotData *InputData, int timeout);
int AfalMbimLinuxRadioSetSync(char *portname, _RadioData *InputData, int timeout);
int AfalMbimLinuxTraceLogSetSync(char *portname, char *InputStr, int *InputData, int timeout);
int LinuxMbimDeviceOpen(const char *portname);
int LinuxMbimDeviceClose(void);

#ifdef __cplusplus
}
#endif

#endif // MBIM_LINUX_HELPER_API_H
