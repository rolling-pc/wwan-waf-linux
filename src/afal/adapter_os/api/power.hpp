#ifndef AFAL_ADAPTER_OS_API_POWER_HPP_
#define AFAL_ADAPTER_OS_API_POWER_HPP_
#include <memory>
#include <QString>
#include <QObject>
namespace afal {
    namespace power {
        enum PowerSupplyType {
            AC,
            DC,
            UNKNOWN,
        };

        enum PowerState {
            SUSPEND   = 1,
            HIBERNATE = 1 << 1,
            RESUME    = 1 << 2,
            REBOOT    = 1 << 3,
            SHUTDOWN  = 1 << 4,
        };

        /**
         * @brief This function get the power supply type.
         * @return The type of power supply.
         */
        PowerSupplyType GetPowerSupplyType();
        /**
         * @brief This function get the battery level.
         * @return The battery level.
         */
        int GetBatteryLevel();
        /**
         * @brief This function control the screen to turn off and on.
         * @param on This param control screen turn on = true, turn off =false.
         * @return Return the execution result, true is success.
         */
        bool SetScreenSwitch(const bool& on);
        /**
         * @brief This function control the system power status.
         * @param powerState This param indicates the state that the system is required to be.
         * @param delayTime This param indicates the time of delayed execution, default is 0.
         * @return Return the execution result, true is success.
         */
        bool SetPowerState(const PowerState& powerState, const int& delayTime = 0);
        /**
         * @brief This function control the system power status.
         * @param targetPower This param indicates the state that required to inhibit,
         *        like SUSPEND|SHUTDOWN(PowerState enum).
         * @return Return the execution result, true is success.
         */
        bool PowerInhibit(const int& targetPower);
        /**
         * @brief This function uninhibit the power state.
         * @return Return the execution result, true is success.
         */
        bool PowerUninhibit();

        /**
         * @brief This define the callback of power state change function.
         */
        using PowerEventHandler = void(*)(PowerState);
        /**
         * @class PowerMonitor
         * @brief This Class used monitor the power state event.
         *
         * PowerMonitor provides functions to start, stop power status event.
         */
        class PowerMonitor {
            public:
                PowerMonitor();
                ~PowerMonitor();
                PowerMonitor(const PowerMonitor&) = delete;
                PowerMonitor& operator = (const PowerMonitor&) = delete;

                /**
                 * @brief This function start monitor power status.
                 * @param handler This param set the power status event handler and start monitor.
                 */
                void StartMonitoring(const PowerEventHandler& handler);
                /**
                 * @brief This function stop monitor power status.
                 */
                void StopMonitoring();
            private:
                class PowerMonitorImpl;
                std::unique_ptr<PowerMonitorImpl> pImpl;
        };

#ifdef _IS_WINDOWS_
        enum ScreenState {
            SCREEN_OFF = 0,
            SCREEN_ON = 1,
            SCREEN_UNKNOWN = 0xFF,
        };

        /**
         * @brief This define the callback of screen state change function.
         */
        using ScreenEventHandler = void(*)(ScreenState);

        /**
         * @class ScreenMonitor
         * @brief This Class used monitor the screen on/off event.
         *
         * ScreenMonitor provides functions to start, stop screen status event.
         */
        class ScreenMonitor {
            public:
                ScreenMonitor();
                ~ScreenMonitor();
                ScreenMonitor(const ScreenMonitor&) = delete;
                ScreenMonitor& operator = (const ScreenMonitor&) = delete;

                /**
                 * @brief This function start monitor screen status.
                 * @param handler This param set the screen status event handler and start monitor.
                 */
                void StartMonitoring(const ScreenEventHandler& handler);
                /**
                 * @brief This function stop monitor screen status.
                 */
                void StopMonitoring();
            private:
                class ScreenMonitorImpl;
                std::unique_ptr<ScreenMonitorImpl> pImpl;
        };
#endif
    }
}

#endif //AFAL_ADAPTER_OS_API_POWER_HPP_