#ifndef __LIBMBIM_COMMON_STTUCT_H__
#define __LIBMBIM_COMMON_STTUCT_H__

struct _RadioData {
    int hw_state;
    int sw_state;
};

struct _SimData {
    int ready_state;
    char iccid[32];
    char local_mccmnc[7];
    char imsi[16];
};

struct _SlotData {
    int ValidSlotNum;
    int ValidExecutorNum;
    int slot_state[2];
    int CurrentSlotIndex;
};

struct _NetworkData {
    int RegisterState;  // deregistered? searching? home? roaming?
    int ConnectState;  // unknown? activated/ing? deactivated/ing?
    char RoamMccmnc[7];
    int rssi;
};

struct _TraceLogData {
    char *InputKey;
    int  InputValue;
};

enum MbimDataType {
    RADIODATA   = 0,
    SIMDATA     = 1,
    SLOTDATA    = 2,
    NETWORKDATA = 3,
    ATCMDDATA   = 4,
    TRACELOG    = 5,
};

void linux_trigger_sim_notify(_SimData *pointer);
void linux_trigger_slot_notify(_SlotData *pointer);
void linux_trigger_radio_notify(_RadioData *pointer);
void linux_trigger_network_notify(_NetworkData *pointer);

#endif
