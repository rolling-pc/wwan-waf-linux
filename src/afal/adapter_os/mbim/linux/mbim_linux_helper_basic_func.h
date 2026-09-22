#ifndef MBIM_LINUX_HELPER_BASIC_FUNC_H
#define MBIM_LINUX_HELPER_BASIC_FUNC_H

#include "libmbim_common_struct.h"

int HelperAfalMbimLinuxSimQuerySync(_SimData *OutputData, int timeout);
int HelperAfalMbimLinuxNetworkQuerySync(_NetworkData *OutputData, int timeout);
int HelperAfalMbimLinuxRadioQuerySync(_RadioData *OutputData, int timeout);
int HelperAfalMbimLinuxSlotQuerySync(_SlotData *OutputData, int timeout);
int HelperAfalMbimLinuxTraceLogQuerySync(char *InputData, int *OutputData, int timeout);
int HelperAfalMbimLinuxAtOverMbimSetSync(char *input_str, char *output_str, int timeout);
int HelperAfalMbimLinuxSlotSetSync(_SlotData *InputData, int timeout);
int HelperAfalMbimLinuxRadioSetSync(_RadioData *InputData, int timeout);
int HelperAfalMbimLinuxTraceLogSetSync(char *InputStr, int *InputData, int timeout);
int HelperLinuxMbimDeviceOpen(char *portname);
int HelperLinuxMbimDeviceClose(void);

#endif // MBIM_LINUX_HELPER_BASIC_FUNC_H
