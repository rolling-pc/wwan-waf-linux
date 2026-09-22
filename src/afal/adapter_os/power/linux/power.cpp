#include "power.hpp"
#include "common.hpp"
#include <dbus/dbus.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <QDebug>
namespace afal {
    namespace power {
        constexpr int TIMEOUT = -1;
        static uint inhibitFd = 0;  // NOLINT

        PowerSupplyType GetPowerSupplyType() {
            // Linux 平台的电源类型查询逻辑，从 `/sys/class/power_supply/AC/online` 读取
            std::ifstream acFile("/sys/class/power_supply/AC/online");
            if(!acFile) {
                return PowerSupplyType::UNKNOWN;
            }

            std::string status;
            if (acFile >> status) {
                return (status == "1") ? PowerSupplyType::AC : PowerSupplyType::DC;
            }

            return PowerSupplyType::UNKNOWN;
        }

        int GetBatteryLevel() {
            // Linux 平台电池电量查询逻辑，实际中从 `/sys/class/power_supply/BAT0/capacity` 获取电量
            std::ifstream batteryFile("/sys/class/power_supply/BAT0/capacity");
            if (!batteryFile) {
                return afal::error::ERR;
            }

            int level = -1;
            if (!(batteryFile >> level)) {
                return afal::error::ERR;
            }

            return level;
        }

        bool SetScreenSwitch(const bool& on) {
            // Linux 平台的屏幕控制逻辑，可以通过 `xset dpms force` 控制显示器
            const std::string COMMAND = on ? "xset dpms force on" : "xset dpms force off";
            return std::system(COMMAND.c_str()) == 0; // NOLINT
        }

        void PowerCommand(const std::string&command, const int& delayTime) {
            int ret = 0;
            std::this_thread::sleep_for(std::chrono::seconds(delayTime));
            ret = std::system(command.c_str());  // NOLINT
            if (!ret) {
                std::cout << "Command error:" << ret << std::endl;
            }
        }

        bool SetRTCWakeup(const int& delayTime) {
            std::ofstream wakeAlarmFile("/sys/class/rtc/rtc0/wakealarm");
            if (!wakeAlarmFile.is_open()) {
                std::cerr << "Error opening wakealarm file" << std::endl;
                return false;
            }

            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            auto wakeup_time = now + std::chrono::seconds(delayTime);
            std::time_t wakeup_time_t = std::chrono::system_clock::to_time_t(wakeup_time);

            wakeAlarmFile << wakeup_time_t;
            wakeAlarmFile.close();
            return true;
        }

        bool SetPowerState(const PowerState& powerState, const int& delayTime) {
            // Linux 平台的电源状态控制逻辑
            std::string command;
            switch (powerState) {
                case SUSPEND:
                    command = "systemctl suspend";
                    break;
                case RESUME:
                    //RTC alarm 定时唤醒OS
                    return SetRTCWakeup(delayTime);
                case REBOOT:
                    command = "systemctl reboot";
                    break;
                case SHUTDOWN:
                    command = "systemctl poweroff";
                    break;
                default:
                    return false;
            }

            if (delayTime == 0) {
                return std::system(command.c_str()) == 0; // NOLINT
            }

            std::thread suspendThread(PowerCommand, command.c_str(), delayTime);
            suspendThread.detach();

            return true;
        }

        bool PowerUninhibit() {
            if (!inhibitFd) {
                return true;
            }

            int ret = true;
            DBusError error;
            dbus_error_init(&error);
            DBusConnection* connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
            if (!connection) {
                dbus_error_free(&error);
                return false;
            }

            DBusMessage* msg = dbus_message_new_method_call("org.freedesktop.login1", 
                                                            "/org/freedesktop/login1", 
                                                            "org.freedesktop.login1.Manager", 
                                                            "Uninhibit");
            dbus_message_append_args(msg, DBUS_TYPE_UNIX_FD, &inhibitFd, DBUS_TYPE_INVALID);  // NOLINT
            DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, msg, 2000, &error);  // NOLINT

            if (reply) {
                if (dbus_error_is_set(&error)) {
                    ret = false;
                    std::cout << "error:" << error.message <<std::endl;
                }
                dbus_message_unref(reply);
            }

            dbus_message_unref(msg);
            dbus_error_free(&error);
            dbus_connection_unref(connection);
            return ret;
        }

        bool PowerInhibit(const int& targetPower) {

            if (!inhibitFd) {
                PowerUninhibit();
            }

            std::string inhibitWhat;
            if (targetPower & PowerState::SUSPEND) {
                inhibitWhat = "sleep";
            }

            if ((targetPower & PowerState::REBOOT) || (targetPower & PowerState::SHUTDOWN)) {
                inhibitWhat = inhibitWhat.empty() ? "shutdown" : inhibitWhat + ":shutdown";
            }

            if (inhibitWhat.empty()) {
                return false;
            }

            bool ret = true;
            DBusError error;
            dbus_error_init(&error);
            DBusConnection* connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
            if (!connection) {
                dbus_error_free(&error);
                return false;
            }

            DBusMessage* msg = dbus_message_new_method_call("org.freedesktop.login1", 
                                                            "/org/freedesktop/login1", 
                                                            "org.freedesktop.login1.Manager", 
                                                            "Inhibit");
            const char* what = inhibitWhat.c_str();
            const char* who = "WWAN service";
            const char* why = "Blocking for WWAN service";
            const char* mode = "block";
            dbus_message_append_args(msg, DBUS_TYPE_STRING, &what,  // NOLINT
                                          DBUS_TYPE_STRING, &who,
                                          DBUS_TYPE_STRING, &why,
                                          DBUS_TYPE_STRING, &mode,
                                          DBUS_TYPE_INVALID);

            //DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, msg, DBUS_TIMEOUT_INFINITE, &error);
            DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, msg, 2000, &error);
            if (reply) {
                dbus_message_get_args(reply, &error, DBUS_TYPE_UNIX_FD, &inhibitFd, DBUS_TYPE_INVALID);  //NOLINT
                if (dbus_error_is_set(&error)) {
                    ret = false;
                    std::cout << "error:" << error.message <<std::endl;
                }
                dbus_message_unref(reply);
            }

            dbus_message_unref(msg);
            dbus_error_free(&error);
            dbus_connection_unref(connection);
            return ret;
        }

        class PowerMonitor::PowerMonitorImpl {
            public:
                PowerMonitorImpl() : running(false) {}

                void Start(PowerEventHandler handler) {
                    std::lock_guard<std::mutex> lock(monitorMutex);
                    if (monitorThread && monitorThread->joinable()) {
                        return;
                    }

                    running = true;
                    eventHandler = handler;
                    monitorThread = std::make_unique<std::thread>(&PowerMonitorImpl::Monitor, this);
                }
                /*
                void Stop() {
                    std::lock_guard<std::mutex> lock(monitorMutex);
                    running = false;

                    if (monitorThread && monitorThread->joinable()) {
                        monitorThread->join();
                        monitorThread.reset();
                    }

                    eventHandler = nullptr;
                }*/
                void Stop() noexcept {
                    try {
                        std::lock_guard<std::mutex> lock(monitorMutex);
                        running = false;
                
                        if (monitorThread) {
                            if (monitorThread->joinable()) {
                                try {
                                    monitorThread->join();
                                } catch (const std::system_error& e) {
                                    qWarning() << "PowerMonitorImpl::Stop join failed:" << e.what();
                                }
                            }
                            monitorThread.reset();
                        }
                
                        eventHandler = nullptr;
                    } catch (const std::exception& e) {
                        qWarning() << "Exception in PowerMonitorImpl::Stop:" << e.what();
                    } catch (...) {
                        qWarning() << "Unknown exception in PowerMonitorImpl::Stop";
                    }
                }

                ~PowerMonitorImpl() {
                    Stop();
                }

            private:
                bool AddRulesToConnection(DBusConnection*& connection) { // NOLINT (readability-convert-member-functions-to-static)
                    DBusError error;
                    dbus_error_init(&error);

                    connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
                    if (!connection) {
                        std::cout << "Connection error: " << error.message << std::endl;
                        dbus_error_free(&error);
                        return false;
                    }

                    dbus_bus_add_match(connection, "type='signal',interface='org.freedesktop.login1.Manager',member='PrepareForSleep'", nullptr);
                    dbus_bus_add_match(connection, "type='signal',interface='org.freedesktop.login1.Manager',member='PrepareForShutdown'", nullptr);
                    //dbus_bus_add_match(connection, "type='signal',interface='org.freedesktop.login1.Manager'", nullptr);
                    dbus_connection_flush(connection);
                    return true;
                }

                void Monitor() {
                    DBusConnection* connection = nullptr;
                    if (!AddRulesToConnection(connection)) {
                        return;
                    }

                    while (running) {
                        dbus_connection_read_write_dispatch(connection, TIMEOUT);
                        DBusMessage* msg = dbus_connection_pop_message(connection);
                        if (msg) {
                            if (dbus_message_is_signal(msg, "org.freedesktop.login1.Manager", "PrepareForSleep")) { //NOLINT
                                bool goToSleep = false;
                                if (!dbus_message_get_args(msg, nullptr, DBUS_TYPE_BOOLEAN, &goToSleep, DBUS_TYPE_INVALID)) { // NOLINT(cppcoreguidelines-pro-type-vararg)
                                    dbus_message_unref(msg);
                                    continue;
                                }

                                if (eventHandler != nullptr) {
                                    goToSleep ? eventHandler(SUSPEND) : eventHandler(RESUME);
                                }
                            } else if (dbus_message_is_signal(msg, "org.freedesktop.login1.Manager", "PrepareForShutdown")) { //NOLINT
                                bool goToShutdown = false;
                                if (!dbus_message_get_args(msg, nullptr, DBUS_TYPE_BOOLEAN, &goToShutdown, DBUS_TYPE_INVALID)) { // NOLINT(cppcoreguidelines-pro-type-vararg)
                                    dbus_message_unref(msg);
                                    continue;
                                }

                                if (eventHandler != nullptr && goToShutdown) {
                                    goToShutdown? eventHandler(REBOOT) : eventHandler(SHUTDOWN);
                                }
                            }
                            dbus_message_unref(msg);
                        }

                        if (!dbus_connection_get_is_connected(connection)) {
                            if (connection) {
                                dbus_connection_unref(connection);
                                connection = nullptr;
                            }
                            if(!AddRulesToConnection(connection)) {
                                return;
                            }
                        }
                    }
                    if (connection) {
                        dbus_connection_unref(connection);
                        connection = nullptr;
                    }
                }

                PowerEventHandler eventHandler = nullptr;
                std::atomic<bool> running;
                std::unique_ptr<std::thread> monitorThread;
                std::mutex monitorMutex;
        };

        PowerMonitor::PowerMonitor() : pImpl(new PowerMonitorImpl()) {}
        PowerMonitor::~PowerMonitor() { StopMonitoring(); }

        void PowerMonitor::StartMonitoring(const PowerEventHandler& handler) {
            pImpl->Start(handler);
        }

        void PowerMonitor::StopMonitoring() {
            pImpl->Stop();
        }
    }
}