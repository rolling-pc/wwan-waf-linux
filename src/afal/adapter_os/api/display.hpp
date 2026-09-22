#ifndef AFAL_ADAPTER_OS_API_DISPLAY_HPP_
#define AFAL_ADAPTER_OS_API_DISPLAY_HPP_
#include <string>
#include <memory>
#ifdef _IS_WINDOWS_
#include <windows.h>
#endif
namespace afal {
    namespace display {
        enum Color {
            RED,
            BLACK,
            YELLOW,
        };

        std::string GetDisplayEnvironment();

        /**
         * @class ProgressBar
         * @brief This Class used to pop a progress bar.
         *
         * ProgressBar provides functions to show, update, and close progress bar.
         */
        class ProgressBar {
            public:
                ProgressBar();
                ~ProgressBar();
                ProgressBar(const ProgressBar&) = delete;
                ProgressBar& operator = (const ProgressBar&) = delete;

                /**
                 * @brief This function show the progress bar.
                 * @param title The title of the progress bar.
                 * @param message The message of the progress bar.
                 * @param color The message color of the progress bar.
                 */
                void Show(const std::string& title, const std::string& message = "", const Color& color = RED);
                /**
                 * @brief This function update the percentage of progress bar.
                 * @param percentage The percentage of the progress bar.
                 * @param message The message of the progress bar, keep the old by default.
                 * @param color The message color of the progress bar,.using red by default.
                 */
                void Update(const int& percentage, const std::string& message = "", const Color& color = RED);
                /**
                 * @brief This function close the progress bar.
                 */
                void Close();
            private:
                class ProgressBarImpl;
                std::unique_ptr<ProgressBarImpl> pImpl;
        };

        /**
         * @class NotificationBox
         * @brief This Class used to pop a notification box.
         *
         * NotificationBox provides functions to show and close notification box.
         */
        class NotificationBox {
            public:
                NotificationBox();
                ~NotificationBox();
                NotificationBox(const NotificationBox&) = delete;
                NotificationBox& operator = (const NotificationBox&) = delete;

                /**
                 * @brief This function show the notification box.
                 * @param title The title of the notification box.
                 * @param message The message of the notification box.
                 * @param color The message color of the notification box.
                 */
                void Show(const std::string& title, const std::string& message = "", const Color& color = BLACK);
                /**
                 * @brief This function close the notification box.
                 */
                void Close();
            private:
                class NotificationBoxImpl;
                std::unique_ptr<NotificationBoxImpl> pImpl;
        };

         /**
         * @class FileExplorer
         * @brief This Class used to pop a file explorer.
         *
         * FileExplorer provides functions to show file explorer.
         */
        class FileExplorer {
            public:
                FileExplorer();
                ~FileExplorer();
                FileExplorer(const FileExplorer&) = delete;
                FileExplorer& operator = (const FileExplorer&) = delete;

                /**
                 * @brief This function show the file explorer.
                 * @param path The default path of the file explorer.
                 */
                int Show(const std::string& path);
                /**
                 * @brief This function close the file explorer.
                 */
                void Close();
            private:
                class FileExplorerImpl;
                std::unique_ptr<FileExplorerImpl> pImpl;
#ifdef _IS_WINDOWS_
                DWORD GetActiveConsoleSessionId();
                static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
                bool CreateProcessInUserSession(const std::wstring& commandLine, DWORD sessionId);
#endif
        };
    }
}
#endif //AFAL_ADAPTER_OS_API_DISPLAY_HPP_