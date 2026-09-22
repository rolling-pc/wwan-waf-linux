#ifndef __MBIM_H__
#define __MBIM_H__

#include <string>
#include <memory>
#include <vector>
#include <list>
#include <algorithm>
#include <iostream>
#include <QMap>
#include <QString>
#include "osinfo.hpp"
#include "global.hpp"

using std::vector;
using std::shared_ptr;

namespace afal {
    namespace mbim {
        enum AfalMbimRadioState {
            MBIM_RADIO_SWITCH_STATE_OFF,
            MBIM_RADIO_SWITCH_STATE_ON
        };

        enum AfalMbimSimState {
            MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED,
            MBIM_SUBSCRIBER_READY_STATE_INITIALIZED,
            MBIM_SUBSCRIBER_READY_STATE_SIM_NOT_INSERTED,
            MBIM_SUBSCRIBER_READY_STATE_BAD_SIM,
            MBIM_SUBSCRIBER_READY_STATE_FAILURE,
            MBIM_SUBSCRIBER_READY_STATE_NOT_ACTIVATED,
            MBIM_SUBSCRIBER_READY_STATE_DEVICE_LOCKED,
            MBIM_SUBSCRIBER_READY_STATE_NO_ESIM_PROFILE
        };

        enum AfalMbimSlotState {
            MBIM_UICC_SLOT_STATE_UNKNOWN,
            MBIM_UICC_SLOT_SATE_OFF_EMPTY,
            MBIM_UICC_SLOT_STATE_OFF,
            MBIM_UICC_SLOT_STATE_EMPTY,
            MBIM_UICC_SLOT_STATE_NOT_READY,
            MBIM_UICC_SLOT_STATE_ACTIVE,
            MBIM_UICC_SLOT_STATE_ERROR,
            MBIM_UICC_SLOT_STATE_ACTIVE_ESIM,
            MBIM_UICC_SLOT_STATE_ACTIVE_ESIM_NO_PROFILES
        };

        enum AfalMbimRegisterState {
            MBIM_REGISTER_STATE_UNKNOWN      = 0,
            MBIM_REGISTER_STATE_DEREGISTERED = 1,
            MBIM_REGISTER_STATE_SEARCHING    = 2,
            MBIM_REGISTER_STATE_HOME         = 3,
            MBIM_REGISTER_STATE_ROAMING      = 4,
            MBIM_REGISTER_STATE_PARTNER      = 5,
            MBIM_REGISTER_STATE_DENIED       = 6
        };

        enum AfalMbimActivationState{
            MBIM_ACTIVATION_STATE_UNKNOWN      = 0,
            MBIM_ACTIVATION_STATE_ACTIVATED    = 1,
            MBIM_ACTIVATION_STATE_ACTIVATING   = 2,
            MBIM_ACTIVATION_STATE_DEACTIVATED  = 3,
            MBIM_ACTIVATION_STATE_DEACTIVATING = 4
        } ;

        enum PCIOTNotificationType
        {
            IOTServiceDefault = 0,//default value
            IOTServiceActive,//start active
            IOTServiceDeactive,//start deactive
            IOTServiceCheck//check
        };

        enum MBIM_DEVICE_STATE
        {
            MBIM_INTERFACE_ADDED = 0,
            MBIM_INTERFACE_REMOVED,
            MBIM_INTERFACE_STATE_MAX
        };

        struct RadioData {
            AfalMbimRadioState hw_state;
            AfalMbimRadioState sw_state;
        };

        typedef struct _MBIM_SET_SYSTEM_TIME
        {
            int sysYear;
            int sysMonth;
            int sysDay;
            int sysHour;
            int sysMinute;
            int sysSecond;
        } MBIM_SET_SYSTEM_TIME;

        typedef struct _MBIM_DUAL_IPC_ELEMENT
        {
            char TypeName[28];
            int Value;
        } MBIM_DUAL_IPC_ELEMENT;

        typedef struct _MBIM_SET_DUAL_IPC
        {
            int ElementCount;
            MBIM_DUAL_IPC_ELEMENT Elements[6];
        } MBIM_SET_DUAL_IPC;

        struct SimData {
            AfalMbimSimState ready_state;
            QString iccid;
            QString local_mccmnc;
            QString imsi;
            SimData() :
                    ready_state(MBIM_SUBSCRIBER_READY_STATE_FAILURE),
                    iccid("ERROR"),
                    local_mccmnc("ERROR"),
                    imsi("ERROR")
            {}
        };

        struct SlotData {
            int ValidSlotNum;
            int ValidExecutorNum;
            AfalMbimSlotState slot_state[2];
            int CurrentSlotIndex;
            SlotData() :
                    ValidSlotNum(-1),
                    ValidExecutorNum(-1),
                    slot_state{ MBIM_UICC_SLOT_STATE_UNKNOWN, MBIM_UICC_SLOT_STATE_UNKNOWN },
                    CurrentSlotIndex(-1)
            {}
        };

        struct NetworkData {
            AfalMbimRegisterState RegisterState;  // deregistered? searching? home? roaming?
            AfalMbimActivationState ConnectState;  // unknown? activated/ing? deactivated/ing?
            QString RoamMccmnc;
            int rssi;
#ifdef _IS_WINDOWS_
            // -1 unknown, 0 no internet access, 1 has internet access (Windows only)
            int HasInternetAccess;
#endif
            NetworkData() :
                    RegisterState(MBIM_REGISTER_STATE_UNKNOWN),
                    ConnectState(MBIM_ACTIVATION_STATE_UNKNOWN),
                    RoamMccmnc("ERROR"),
                    rssi(99)
#ifdef _IS_WINDOWS_
                    , HasInternetAccess(-1)
#endif
            {}
        };

        struct PCIOTData {
            PCIOTNotificationType NotificationType;  // Active? Deactive? Activating?
            QString IndicateMessage;
            PCIOTData() :
                    NotificationType(IOTServiceDefault),
                    IndicateMessage("")
            {}
        };

        struct MBIMInterfaceData {
            MBIM_DEVICE_STATE NotificationType;  // Active? Deactive? Activating?
            MBIMInterfaceData() :
                    NotificationType(MBIM_INTERFACE_STATE_MAX)
            {}
        };

        enum AfalMbimWatcherType {
            RADIO_WATCHER, SIM_WATCHER, SLOT_WATCHER, NETWORK_WATCHER, PCIOT_WATCHER,MBIMINTERFACE_WATCHER
        };

        class watcher {
        protected:
            AfalMbimWatcherType watcher_type;
        public:
            int owner_id;
            int get_watcher_type();
            virtual ~watcher() = default; // 添加虚析构函数
            watcher() :
                    watcher_type(RADIO_WATCHER),
                    owner_id(-1)
            {}
        };

        class CellularRadioWatcher : public watcher {
        public:
            CellularRadioWatcher();

            virtual void notify(RadioData notifydata) = 0; // 纯虚函数，留给调用者实现
        };

        class CellularSimWatcher : public watcher {
        public:
            CellularSimWatcher();

            virtual void notify(SimData notifydata) = 0; // 纯虚函数，留给调用者实现
        };

        class CellularSlotWatcher : public watcher {
        public:
            CellularSlotWatcher();

            virtual void notify(SlotData notifydata) = 0; // 纯虚函数，留给调用者实现
        };

        class CellularNetworkWatcher : public watcher {
        public:
            CellularNetworkWatcher();

            virtual void notify(NetworkData notifydata) = 0; // 纯虚函数，留给调用者实现
        };

        class CellularPCIOTWatcher : public watcher {
            public:
                CellularPCIOTWatcher();
    
                virtual void notify(PCIOTData notifydata) = 0; // 纯虚函数，留给调用者实现
        };    

        class MBIMInterfaceWatcher : public watcher {
            public:
            MBIMInterfaceWatcher();
    
                virtual void notify(MBIMInterfaceData notifydata) = 0; // 纯虚函数，留给调用者实现
        };   

        class MbimDevice {
        private:
            vector <shared_ptr<watcher>> SimWatcherList;
            vector <shared_ptr<watcher>> SlotWatcherList;
            vector <shared_ptr<watcher>> NetworkWatcherList;
            vector <shared_ptr<watcher>> RadioWatcherList;
            vector <shared_ptr<watcher>> PciotWatcherList;
            vector <shared_ptr<watcher>> MbimInterfaceWatcherList;
            
            // private constructor, refuse user to create the mbim instance directly.
            MbimDevice();
            ~MbimDevice() {}
            // forbidden copy constructor and "=" operator.
            MbimDevice(const MbimDevice&) = delete;
            MbimDevice& operator=(const MbimDevice&) = delete;
        public:
            QString PortName;
            // the first step to use mbim lib, get instance.
            static MbimDevice &getInstance();
            // the second step to use mbim lib, init mbim device for further use.
            int init(QString InputPortName);
            // the last step to use mbim lib, deinit mbim device and won't use it anymore.
            int deinit(void);
            // Join the process-lifetime MTA. Call from a long-lived thread
            // (service/Qt main) before any worker calls init().
            static void ensureProcessApartment();
            // False after deinit has started; MBN sinks should no-op.
            static bool notificationsAllowed();
            // False after Win32 Removal (or Close) until Arrival / WinRT Open.
            // WinRT QuerySync must not run while this is false.
            static bool interfaceAlive();
            // Win32 Removal: pause query gate, drain in-flight. No WinRT calls.
            static void beginInterfaceRemoval();
            // Win32 Arrival: mark Win32 interface alive. Does not start WinRT.
            static void endInterfaceArrival();
            // register event to mbim device, once event occurred, watcher's notify function will be triggered.
            int AfalMbimRegisterNotification(const shared_ptr<watcher> &w);
            // deregister event from mbim device.
            int AfalMbimDeregisterNotification(const shared_ptr<watcher> &w);
            // notify callback's function is to find the specific list and call every list's notify function.
            int AfalMbimExecuteNotify(AfalMbimWatcherType func_type, void *data);

            // network function.
            int AfalMbimNetworkQuerySync(NetworkData &OutputData, int timeout);
            // no set message on network function.

            // SIM function.
            int AfalMbimSimQuerySync(SimData &OutputData, int timeout);
            // no set message on SIM function.

            // AT over MBIM function.
            // no query message on AT over MBIM function.
            int AfalMbimAtOverMbimSetSync(QString InputStr, QString &OutputStr, int timeout);

            // Intel Trace log function.
            int AfalMbimTraceLogQuerySync(QMap<QString, int> &OutputData, int timeout);
            int AfalMbimTraceLogSetSync(QMap<QString, int> &InputData, int timeout);

            // radio function.
            int AfalMbimRadioSetSync(RadioData &InputData, int timeout);
            int AfalMbimRadioQuerySync(RadioData &OutputData, int timeout);

            // slot function.
            int AfalMbimSlotQuerySync(SlotData &OutputData, int timeout);
            int AfalMbimSlotSetSync(SlotData &InputData, int timeout);

            //mbim reboot - for 350 platform
            int AfalMbimMBIMRebootSetSync(int BootMode, int timeout);
            //trace systime setting
            int AfalMbimTraceSystimeSetSync(MBIM_SET_SYSTEM_TIME SystemTime, int timeout);
            //trace systime setting
            int AfalMbimDebugPortSetSync(int portNum, int portStatus, int timeout);
            //dipc set
            int AfalMbimDIPCSetSync(MBIM_SET_DUAL_IPC msdi, int timeout);
            //device mode
            int AfalDeviceModeGetSync(afal::osinfo::DeviceModeData *deviceModeData, int timeout);
#ifdef _IS_WINDOWS_
            //win32 AT Tunnel
            int AfalMbimAtOverMbimWin32SetSync(QString InputStr, QString& OutputStr, int timeout); 
            int AfalMbimPCIOTTunnelStart(); 
            int AfalMbimPCIOTTunnelStop(); 
#endif
        };
    }
}

#endif
