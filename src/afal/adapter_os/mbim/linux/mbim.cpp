#include <iostream>
#include "mbim.hpp"
#include "log.hpp"
#include "common.hpp"
#include "version.h"
#include <string>
#include <mutex>
#include "mbim_linux_helper_api.h"
#include "libmbim_common_struct.h"

using namespace afal::log;
using namespace afal::mbim;
using namespace afal::error;

std::mutex mtx;

/* -------------------Begin Notification related functions------------------- */
// pay attention to free the malloc space here.
void linux_trigger_sim_notify(_SimData *pointer) {
    int         ret        = ERR;
    MbimDevice& mbimDevice = afal::mbim::MbimDevice::getInstance();

    if (!pointer) {
        LOG_ERROR("NULL pointer!\n");
        return;
    }

    ret = mbimDevice.AfalMbimExecuteNotify(SIM_WATCHER, pointer);
    if (ret != OK) {
        LOG_ERROR("execute failed!\n");
    }

    if (pointer)
    {
        memset(pointer, 0, sizeof(_SimData));
        //free(pointer);
    }

    pointer = NULL;
    return;
}

void linux_trigger_slot_notify(_SlotData *pointer) {
    int         ret        = ERR;
    MbimDevice& mbimDevice = afal::mbim::MbimDevice::getInstance();

    if (!pointer) {
        LOG_ERROR("NULL pointer!\n");
        return;
    }

    ret = mbimDevice.AfalMbimExecuteNotify(SLOT_WATCHER, pointer);
    if (ret != OK) {
        LOG_ERROR("execute failed!\n");
    }

    if (pointer)
    {
        memset(pointer, 0, sizeof(_SlotData));
        //free(pointer);
    }

    pointer = NULL;
    return;
}

void linux_trigger_radio_notify(_RadioData *pointer) {
    int         ret        = ERR;
    MbimDevice& mbimDevice = afal::mbim::MbimDevice::getInstance();

    if (!pointer) {
        LOG_ERROR("NULL pointer!\n");
        return;
    }

    ret = mbimDevice.AfalMbimExecuteNotify(RADIO_WATCHER, pointer);
    if (ret != OK) {
        LOG_ERROR("execute failed!\n");
    }

    if (pointer)
    {
        memset(pointer, 0, sizeof(_RadioData));
        //free(pointer);
    }
    pointer = NULL;
    return;
}

void linux_trigger_network_notify(_NetworkData *pointer) {
    int         ret        = ERR;
    MbimDevice& mbimDevice = afal::mbim::MbimDevice::getInstance();

    if (!pointer) {
        LOG_ERROR("NULL pointer!\n");
        return;
    }

    ret = mbimDevice.AfalMbimExecuteNotify(NETWORK_WATCHER, pointer);
    if (ret != OK) {
        LOG_ERROR("execute failed!\n");
    }
   
    if (pointer)
    {
        memset(pointer, 0, sizeof(_NetworkData));
        //free(pointer);
    }

    pointer = NULL;
    return;
}

int MbimDevice::AfalMbimExecuteNotify(AfalMbimWatcherType func_type, void *data) {
    vector<shared_ptr<watcher>>* watcherList     = nullptr;

    switch (func_type) {
        case RADIO_WATCHER: {
            watcherList = &RadioWatcherList;
            RadioData OutputData;
            _RadioData *pointer = (_RadioData *)data;
            OutputData.sw_state = (AfalMbimRadioState)pointer->sw_state;
            OutputData.hw_state = (AfalMbimRadioState)pointer->hw_state;

            // 在watcherList中遍历每一个watcher成员，然后分别调用它们的notify函数。
            for (auto& watcher : *watcherList)
            {
                auto cellularWatcher =
                    std::dynamic_pointer_cast<CellularRadioWatcher>(watcher);
                if (watcher)
                {
                    cellularWatcher->notify(OutputData);
                }
            }
            break;
        }
        case SIM_WATCHER: {
            watcherList = &SimWatcherList;
            SimData OutputData;
            _SimData *pointer       = (_SimData *)data;
            OutputData.ready_state  = (AfalMbimSimState)pointer->ready_state;
            OutputData.iccid        = QString(QLatin1String(pointer->iccid));
            OutputData.local_mccmnc = QString(QLatin1String(pointer->local_mccmnc));
            OutputData.imsi         = QString(QLatin1String(pointer->imsi));

            // 在watcherList中遍历每一个watcher成员，然后分别调用它们的notify函数。
            for (auto& watcher : *watcherList)
            {
                auto cellularWatcher =
                    std::dynamic_pointer_cast<CellularSimWatcher>(watcher);
                if (watcher)
                {
                    cellularWatcher->notify(OutputData);
                }
            }
            break;
        }
        case SLOT_WATCHER: {
            watcherList = &SlotWatcherList;
            SlotData OutputData;
            _SlotData *pointer            = (_SlotData *)data;
            OutputData.slot_state[0]      = (AfalMbimSlotState)pointer->slot_state[0];
            OutputData.slot_state[1]      = (AfalMbimSlotState)pointer->slot_state[1];
            OutputData.ValidExecutorNum   = pointer->ValidExecutorNum;
            OutputData.CurrentSlotIndex   = pointer->CurrentSlotIndex;
            OutputData.ValidSlotNum       = pointer->ValidSlotNum;
            // 在watcherList中遍历每一个watcher成员，然后分别调用它们的notify函数。
            for (auto& watcher : *watcherList)
            {
                auto cellularWatcher =
                        std::dynamic_pointer_cast<CellularSlotWatcher>(watcher);
                if (watcher)
                {
                    cellularWatcher->notify(OutputData);
                }
            }
            break;
        }
        case NETWORK_WATCHER: {
            watcherList = &NetworkWatcherList;
            NetworkData OutputData;
            _NetworkData *pointer     = (_NetworkData *)data;
            OutputData.RegisterState  = (AfalMbimRegisterState)pointer->RegisterState;
            OutputData.ConnectState   = (AfalMbimActivationState)pointer->rssi;
            OutputData.RoamMccmnc     = QString(QLatin1String(pointer->RoamMccmnc));
            OutputData.rssi           = pointer->rssi;
            // 在watcherList中遍历每一个watcher成员，然后分别调用它们的notify函数。
            for (auto& watcher : *watcherList)
            {
                auto cellularWatcher =
                        std::dynamic_pointer_cast<CellularNetworkWatcher>(watcher);
                if (watcher)
                {
                    cellularWatcher->notify(OutputData);
                }
            }
            break;
        }
        default:
            LOG_ERROR("Unknown type:%d\n", func_type);
            return ERR;
    }

    LOG_DEBUG("%s finished!\n", __func__);
    return OK;
}
/* -------------------End Notification related functions------------------- */

/* -------------------Begin watcher class related functions------------------- */
int watcher::get_watcher_type(){
    return watcher_type;
}

CellularRadioWatcher::CellularRadioWatcher() {
    watcher_type = RADIO_WATCHER;
}

CellularSimWatcher::CellularSimWatcher() {
    watcher_type = SIM_WATCHER;
}

CellularSlotWatcher::CellularSlotWatcher() {
    watcher_type = SLOT_WATCHER;
}

CellularNetworkWatcher::CellularNetworkWatcher() {
    watcher_type = NETWORK_WATCHER;
}

CellularPCIOTWatcher::CellularPCIOTWatcher() {
    watcher_type = PCIOT_WATCHER;
}

MBIMInterfaceWatcher::MBIMInterfaceWatcher() {
    watcher_type = MBIMINTERFACE_WATCHER;
}

/* -------------------End watcher class related functions------------------- */

/* -------------------Begin MbimDevice class related functions------------------- */
MbimDevice& MbimDevice::getInstance() {
    static MbimDevice instance;
    return instance;
}

MbimDevice::MbimDevice() {
    Logger::Instance().Init(CONSOLE, "afal");
    LOG_DEBUG("mbim lib version :%s\n", MBIM_VERSION_STRING);
}

int MbimDevice::init(QString InputPortName) {
    int ret = ERR;

    if (InputPortName.isEmpty()) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }

    PortName = InputPortName;
    std::string TmpPortName = InputPortName.toStdString();

    std::unique_lock<std::mutex> lock(mtx);
    ret = LinuxMbimDeviceOpen(TmpPortName.c_str());
    if (ret != OK) {
        LOG_ERROR("Init failed!\n");
        return ERR;
    }
    LOG_DEBUG("Init finished!\n");
    return OK;
}

int MbimDevice::deinit(void) {
    int ret = ERR;

    std::unique_lock<std::mutex> lock(mtx);

    ret = LinuxMbimDeviceClose();
    if (ret != OK) {
        LOG_ERROR("Init failed!\n");
        return ERR;
    }

    PortName.clear();
    LOG_DEBUG("Deinit finished!\n");
    return OK;
}

void MbimDevice::ensureProcessApartment() {
}

bool MbimDevice::notificationsAllowed() {
    return true;
}

bool MbimDevice::interfaceAlive() {
    return true;
}

void MbimDevice::beginInterfaceRemoval() {
}

void MbimDevice::endInterfaceArrival() {
}

int MbimDevice::AfalMbimSimQuerySync(afal::mbim::SimData &OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);

    int ret = ERR;
    if (timeout < 0 || timeout > 30) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }
    if (timeout == 0) {
        LOG_ERROR("set default timeout 5!\n");
        timeout = 5;
    }

    std::unique_lock<std::mutex> lock(mtx);

    _SimData TmpOutputData;
    memset(&TmpOutputData, 0, sizeof(_SimData));
    std::string TmpPortName = PortName.toStdString();

    ret = AfalMbimLinuxSimQuerySync((char *)TmpPortName.c_str(), &TmpOutputData, timeout);
    if (ret != OK) {
        LOG_ERROR("AfalMbimLinuxSimQuerySync failed!\n");
        return ERR;
    }

    OutputData.ready_state  = (AfalMbimSimState)TmpOutputData.ready_state;
    OutputData.iccid        = QString(QLatin1String(TmpOutputData.iccid));
    OutputData.local_mccmnc = QString(QLatin1String(TmpOutputData.local_mccmnc));
    OutputData.imsi         = QString(QLatin1String(TmpOutputData.imsi));

    LOG_DEBUG("imsi: %s\n", OutputData.imsi.toUtf8().data());
    LOG_DEBUG("iccid: %s\n", OutputData.iccid.toUtf8().data());
    LOG_DEBUG("local_mccmnc: %s\n", OutputData.local_mccmnc.toUtf8().data());
    return OK;
}

int MbimDevice::AfalMbimNetworkQuerySync(afal::mbim::NetworkData &OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);

    int ret = ERR;
    if (timeout < 0 || timeout > 30) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }
    if (timeout == 0) {
        LOG_ERROR("set default timeout 5!\n");
        timeout = 5;
    }

    std::unique_lock<std::mutex> lock(mtx);

    _NetworkData TmpOutputData;
    memset(&TmpOutputData, 0, sizeof(_NetworkData));
    // keep rssi default value as 99, means no measure yet or unknown.
    TmpOutputData.rssi = 99;
    std::string TmpPortName = PortName.toStdString();

    ret = AfalMbimLinuxNetworkQuerySync((char *)TmpPortName.c_str(), &TmpOutputData, timeout);
    if (ret != OK) {
        LOG_ERROR("AfalMbimLinuxNetworkQuerySync failed!\n");
        return ERR;
    }

    OutputData.RegisterState  = (AfalMbimRegisterState)TmpOutputData.RegisterState;
    OutputData.ConnectState   = (AfalMbimActivationState)TmpOutputData.rssi;
    OutputData.RoamMccmnc     = QString(QLatin1String(TmpOutputData.RoamMccmnc));
    OutputData.rssi           = TmpOutputData.rssi;

    LOG_DEBUG("RegisterState: %d\n", OutputData.RegisterState);
    LOG_DEBUG("ConnectState: %d\n", OutputData.ConnectState);
    LOG_DEBUG("RoamMccmnc: %s\n", OutputData.RoamMccmnc.toUtf8().data());
    LOG_DEBUG("rssi: %d\n", OutputData.rssi);
    return OK;
}

int MbimDevice::AfalMbimRadioQuerySync(afal::mbim::RadioData &OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);

    int ret = ERR;
    if (timeout < 0 || timeout > 30) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }
    if (timeout == 0) {
        LOG_ERROR("set default timeout 5!\n");
        timeout = 5;
    }

    std::unique_lock<std::mutex> lock(mtx);

    _RadioData TmpOutputData;
    memset(&TmpOutputData, 0, sizeof(_RadioData));
    std::string TmpPortName = PortName.toStdString();

    ret = AfalMbimLinuxRadioQuerySync((char *)TmpPortName.c_str(), &TmpOutputData, timeout);
    if (ret != OK) {
        LOG_ERROR("%s failed!\n", __func__);
        return ERR;
    }

    OutputData.hw_state  = (AfalMbimRadioState)TmpOutputData.hw_state;
    OutputData.sw_state  = (AfalMbimRadioState)TmpOutputData.sw_state;

    return OK;
}

int MbimDevice::AfalMbimSlotQuerySync(afal::mbim::SlotData &OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);

    int ret = ERR;
    if (timeout < 0 || timeout > 30) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }
    if (timeout == 0) {
        LOG_ERROR("set default timeout 5!\n");
        timeout = 5;
    }

    std::unique_lock<std::mutex> lock(mtx);

    _SlotData TmpOutputData;
    memset(&TmpOutputData, 0, sizeof(_SlotData));
    std::string TmpPortName = PortName.toStdString();

    ret = AfalMbimLinuxSlotQuerySync((char *)TmpPortName.c_str(), &TmpOutputData, timeout);
    if (ret != OK) {
        LOG_ERROR("%s failed!\n", __func__);
        return ERR;
    }

    OutputData.ValidSlotNum     = TmpOutputData.ValidSlotNum;
    OutputData.ValidExecutorNum = TmpOutputData.ValidExecutorNum;
    OutputData.CurrentSlotIndex = TmpOutputData.CurrentSlotIndex;
    OutputData.slot_state[0]    = (AfalMbimSlotState)TmpOutputData.slot_state[0];
    OutputData.slot_state[1]    = (AfalMbimSlotState)TmpOutputData.slot_state[1];

    return OK;
}

int MbimDevice::AfalMbimTraceLogQuerySync(QMap<QString, int> &OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);

    int ret = ERR;
    if (timeout < 0 || timeout > 30) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }
    if (timeout == 0) {
        LOG_ERROR("set default timeout 5!\n");
        timeout = 5;
    }

    std::unique_lock<std::mutex> lock(mtx);

    std::string                        TmpPortName   = PortName.toStdString();
    int                                request_num   = OutputData.size();
    QMap<QString, int>::const_iterator it            = OutputData.constBegin();
    QString                            KeyStr;
    std::string                        TmpInputStr;
    int                                TmpOutputData = ERR;
    int                                fail_flag     = 0;

    if (request_num < 1) {
        LOG_ERROR("%s failed!\n", __func__);
        return ERR;
    }

    for (int i = 0; i < request_num; i++, it++) {
        if (it != OutputData.constEnd()) {
            KeyStr = it.key();
            TmpInputStr = KeyStr.toStdString();
            TmpOutputData = ERR;
        }
        else {
            LOG_DEBUG("Reach the QMap's tail!\n");
            break;
        }

        ret = AfalMbimLinuxTraceLogQuerySync((char *)TmpPortName.c_str(), (char *)TmpInputStr.c_str(), &TmpOutputData, timeout);
        if (ret != OK || TmpOutputData < 0) {
            LOG_ERROR("%s failed!\n", __func__);
            fail_flag = 1;
            continue;
        }

        LOG_DEBUG("%s value: %d\n", (KeyStr.toStdString()).c_str(), TmpOutputData);
        if (OutputData.contains(KeyStr)) {
            OutputData[KeyStr] = TmpOutputData;
        }
        else {
            LOG_ERROR("%s finished, but can't modify the key-value pair!\n", __func__);
            fail_flag = 1;
            continue;
        }
    }

    if (fail_flag)
        return ERR;
    else
        return OK;
}

int MbimDevice::AfalMbimAtOverMbimSetSync(QString InputStr, QString &OutputStr, int timeout) {
    if (InputStr.isEmpty() || timeout < 0 || timeout > 30) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }
    if (timeout == 0) {
        LOG_ERROR("set default timeout 5!\n");
        timeout = 10;
    }
    LOG_DEBUG("enter %s!\n", __func__);

    std::unique_lock<std::mutex> lock(mtx);

    std::string TmpInputStr   = InputStr.toStdString();
    std::string TmpPortName   = PortName.toStdString();
    int         ret           = ERR;

    constexpr size_t OUT_BUF_SIZE = 16 * 1024;
    char* TmpOutputStr = (char*)malloc(OUT_BUF_SIZE);

    if (TmpOutputStr == NULL) {
        LOG_ERROR("malloc failed!\n");
        OutputStr = OutputStr + "ERROR";
        return ERR;
    }
    memset(TmpOutputStr, 0, OUT_BUF_SIZE);

    ret = AfalMbimLinuxAtOverMbimSetSync((char *)TmpPortName.c_str(), (char *)TmpInputStr.c_str(), TmpOutputStr, timeout);
   
    if (ret != OK || strlen(TmpOutputStr) < 1) {
        LOG_ERROR("send command failed!\n");
        OutputStr = OutputStr + "ERROR";
        free(TmpOutputStr);
        TmpOutputStr = NULL;
        return ERR;
    }

    // The modem itself answered with a definitive final "ERROR" result code
    // (no +CME detail). This is a rejection by the modem, not a transport
    // failure: report it with a distinct code so callers can avoid useless
    // retries. OutputStr still carries "ERROR" for existing parsing.
    if (strstr(TmpOutputStr, "ERROR") != nullptr && strstr(TmpOutputStr, "+CME") == nullptr) {
        LOG_ERROR("AT command rejected by modem (ERROR)!\n");
        OutputStr = OutputStr + "ERROR";
        free(TmpOutputStr);
        TmpOutputStr = NULL;
        return ERR_AT_ERROR;
    }

    LOG_DEBUG("output data:%s\n", TmpOutputStr);
    OutputStr = QString(QLatin1String(TmpOutputStr));
    free(TmpOutputStr);
    TmpOutputStr = NULL;
    return OK;
}

int MbimDevice::AfalMbimSlotSetSync(afal::mbim::SlotData &InputData, int timeout) {
    int ret = ERR;

    if (timeout < 0 || timeout > 30) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }
    if (timeout == 0) {
        LOG_ERROR("set default timeout 5!\n");
        timeout = 5;
    }
    LOG_DEBUG("enter %s!\n", __func__);

    std::unique_lock<std::mutex> lock(mtx);

    _SlotData TmpInputData;
    memset(&TmpInputData, 0, sizeof(_SlotData));

    TmpInputData.CurrentSlotIndex = InputData.CurrentSlotIndex;

    std::string TmpPortName = PortName.toStdString();

    ret = AfalMbimLinuxSlotSetSync((char *)TmpPortName.c_str(), &TmpInputData, timeout);
    if (ret != OK) {
        LOG_ERROR("%s failed!\n", __func__);
        return ERR;
    }
    else if (TmpInputData.CurrentSlotIndex != InputData.CurrentSlotIndex) {
        LOG_ERROR("%s: Expect: %d, Actual: %d\n", __func__, InputData.CurrentSlotIndex, TmpInputData.CurrentSlotIndex);
        return ERR;
    }
    else {
        LOG_DEBUG("%s execute succeed!\n", __func__);
        return OK;
    }
}

int MbimDevice::AfalMbimRadioSetSync(afal::mbim::RadioData &InputData, int timeout) {
    int ret = ERR;

    if (timeout < 0 || timeout > 30) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }
    if (timeout == 0) {
        LOG_ERROR("set default timeout 5!\n");
        timeout = 5;
    }
    LOG_DEBUG("enter %s!\n", __func__);

    std::unique_lock<std::mutex> lock(mtx);

    _RadioData TmpInputData;
    memset(&TmpInputData, 0, sizeof(_RadioData));
    TmpInputData.sw_state = InputData.sw_state;
    std::string TmpPortName = PortName.toStdString();

    // there is no way to change the HW radio state from host, so internal function only deal with HW radio state.
    ret = AfalMbimLinuxRadioSetSync((char *)TmpPortName.c_str(), &TmpInputData, timeout);
    if (ret != OK) {
        LOG_ERROR("%s failed!\n", __func__);
        return ERR;
    }
    else if (TmpInputData.sw_state != InputData.sw_state) {
        LOG_ERROR("%s: Expect: %d, Actual: %d\n", __func__, InputData.sw_state, TmpInputData.sw_state);
        return ERR;
    }
    else {
        LOG_DEBUG("%s execute succeed!\n", __func__);
        return OK;
    }
}

int MbimDevice::AfalMbimTraceLogSetSync(QMap<QString, int> &InputData, int timeout) {
    int ret = ERR;

    if (timeout < 0 || timeout > 30) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }
    if (timeout == 0) {
        LOG_ERROR("set default timeout 5!\n");
        timeout = 5;
    }
    LOG_DEBUG("enter %s!\n", __func__);

    std::unique_lock<std::mutex> lock(mtx);

    std::string                        TmpPortName   = PortName.toStdString();
    int                                request_num   = InputData.size();
    QMap<QString, int>::const_iterator it            = InputData.constBegin();
    QString                            KeyStr;
    std::string                        TmpInputStr;
    int                                TmpInputData  = ERR;
    int                                fail_flag     = 0;

    if (request_num < 1) {
        LOG_ERROR("%s failed!\n", __func__);
        return ERR;
    }

    for (int i = 0; i < request_num; i++, it++) {
        if (it != InputData.constEnd()) {
            KeyStr = it.key();
            TmpInputStr = KeyStr.toStdString();
            TmpInputData = InputData[KeyStr];
        }
        else {
            LOG_DEBUG("Reach the QMap's tail!\n");
            break;
        }

        ret = AfalMbimLinuxTraceLogSetSync((char *)TmpPortName.c_str(), (char *)TmpInputStr.c_str(), &TmpInputData, timeout);
        if (ret != OK) {
            LOG_ERROR("%s failed!\n", __func__);
            InputData[KeyStr] = ERR;
            fail_flag = 1;
            continue;
        }

        LOG_DEBUG("New %s value: %d\n", (KeyStr.toStdString()).c_str(), TmpInputData);
        if (TmpInputData == InputData[KeyStr]) {
            LOG_DEBUG("Set succeed!\n");
        }
        else {
            LOG_ERROR("%s: key %s: Expect: %d, Actual: %d\n", __func__, KeyStr.toStdString(), InputData[KeyStr], TmpInputData);
            fail_flag = 1;
            continue;
        }
    }

    if (fail_flag)
        return ERR;
    else
        return OK;
}

int MbimDevice::AfalMbimRegisterNotification(const shared_ptr<watcher> &w){
    // 确定使用哪个列表
    vector<shared_ptr<watcher>>* watcherList = nullptr;

    switch (w->get_watcher_type()) {
        case RADIO_WATCHER:
            watcherList = &RadioWatcherList;
            break;
        case SIM_WATCHER:
            watcherList = &SimWatcherList;
            break;
        case SLOT_WATCHER:
            watcherList = &SlotWatcherList;
            break;
        case NETWORK_WATCHER:
            watcherList = &NetworkWatcherList;
            break;
        default:
            LOG_ERROR("Unknown watcher type.");
            return ERR;
    }

    // 检查是否已经存在具有相同 owner_id 的 watcher
    auto it = std::find_if(watcherList->begin(), watcherList->end(),
                           [&w](const shared_ptr<watcher>& existingWatcher) {
                               return existingWatcher->owner_id == w->owner_id;
                           });

    // 如果找到相同 owner_id 的 watcher，则打印异常
    if (it != watcherList->end()) {
        LOG_ERROR("Watcher with owner_id %d is already registered.", w->owner_id);
        return OK;
    }

    // 没有找到相同 owner_id 的 watcher，将新 watcher 添加到列表中
    watcherList->push_back(w);
    LOG_DEBUG("Watcher with owner_id %d added successfully.", w->owner_id);
    return OK;
}

int MbimDevice::AfalMbimDeregisterNotification(const shared_ptr<watcher> &w) {
    // 确定使用哪个列表
    vector<shared_ptr<watcher>>* watcherList = nullptr;

    switch (w->get_watcher_type()) {
        case RADIO_WATCHER:
            watcherList = &RadioWatcherList;
            break;
        case SIM_WATCHER:
            watcherList = &SimWatcherList;
            break;
        case SLOT_WATCHER:
            watcherList = &SlotWatcherList;
            break;
        case NETWORK_WATCHER:
            watcherList = &NetworkWatcherList;
            break;
        default:
            LOG_ERROR("Unknown watcher type.");
            return ERR;
    }

    // 在相应的列表中查找具有相同 owner_id 的 watcher
    auto it = std::find_if(watcherList->begin(), watcherList->end(),
                           [&w](const shared_ptr<watcher>& existingWatcher) {
                               return existingWatcher->owner_id == w->owner_id;
                           });

    // 如果找到了匹配的 watcher，则移除
    if (it != watcherList->end()) {
        watcherList->erase(it);
        LOG_DEBUG("Watcher type: %d with owner_id %d removed successfully.", w->get_watcher_type(), w->owner_id);
        return OK;
    } else {
        LOG_ERROR("Watcher type: %d with owner_id %d not found.", w->get_watcher_type(), w->owner_id);
        return OK;
    }
}

int MbimDevice::AfalMbimMBIMRebootSetSync(int BootMode, int timeout) {
    int ret = ERR;
    return ret;
}

int MbimDevice::AfalMbimDebugPortSetSync(int portNum, int portStatus, int timeout) {
    int ret = ERR;
    return ret;
}

int MbimDevice::AfalMbimTraceSystimeSetSync(MBIM_SET_SYSTEM_TIME SystemTime, int timeout) {
    int ret = ERR;
    return ret;
}

int MbimDevice::AfalMbimDIPCSetSync(MBIM_SET_DUAL_IPC msdi, int timeout) {
    int ret = ERR;
    return ret;
}

int AfalDeviceModeGetSync(afal::osinfo::DeviceModeData *deviceModeData, int timeout){
    int ret = ERR;
    return ret;
}