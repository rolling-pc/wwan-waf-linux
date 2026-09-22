extern "C" {
    #include "progress.h"
}
#include "display.hpp"
#include "osinfo.hpp"
#include <iostream>
#include <sys/stat.h>

namespace afal {
    using namespace afal::osinfo;
    namespace display {
        /**
         * @class ProgressBar
         * @brief This Class used to pop a progress bar.
         *
         * ProgressBar provides functions to show, update, and close progress bar.
         */
        class ProgressBar::ProgressBarImpl {
            public:
                void Show(const std::string& title, const std::string& message, const Color& color) {
                    msgColor = color;
                    barTitle = title;
                    barMsg = message;

                    enum CurrentDistibId hostType = CurrentDistibId::Ubuntu;
                    fibocom_get_current_distrib_id(&hostType);
                    if (progressBar == nullptr) {
                        progressBar = CreateProgressImpl(hostType);
                        if (progressBar == nullptr) {
                            return;
                        }
                    }

                    progressBar->fibocom_get_progress_environment_variable(progressBar);
                    progressBar->fibocom_set_progress_title(progressBar, barTitle.c_str());
                    std::snprintf(progressBar->progressText, sizeof(progressBar->progressText), "%s", barMsg.c_str());  // NOLINT
                    progressBar->fibocom_set_progress_init_text(progressBar);
                    progressBar->fibocom_set_progress_schedule(progressBar, 1);
                    progressBar->fibocom_start_progress(progressBar);
                }

                void Update(const int& percentage, const std::string& message, const Color& color) {
                    if (progressBar == nullptr) {
                        return;
                    }

                    msgColor = color;
                    if (!message.empty()) {
                        barMsg = message;
                    }
                    std::cout << "ProgressBar::barMsg: " << barMsg << std::endl;
                    std::cout << "ProgressBar::percentage: " << percentage << std::endl;
                    std::snprintf(progressBar->progressText, sizeof(progressBar->progressText), "%s", barMsg.c_str());  // NOLINT
                    progressBar->fibocom_set_progress_schedule(progressBar, percentage);
                    progressBar->fibocom_refresh_progress(progressBar, barMsg.c_str(), percentage);
                }

                void Close() {
                    if (progressBar == nullptr) {
                        return;
                    }

                    progressBar->fibocom_close_progress(progressBar);
                    DestroyProgressImpl(progressBar);
                }
            private:
                Color msgColor = RED;
                std::string barMsg;
                std::string barTitle;
                Progress* progressBar = nullptr;
        };

        ProgressBar::ProgressBar() : pImpl(std::make_unique<ProgressBarImpl>()) {}
        ProgressBar::~ProgressBar() = default;

        void ProgressBar::Show(const std::string& title, const std::string& message, const Color& color) {
            pImpl->Show(title, message, color);
        }

        void ProgressBar::Update(const int& percentage, const std::string& message, const Color& color) {
            pImpl->Update(percentage, message, color);
        }

        void ProgressBar::Close() {
            pImpl->Close();
        }

        /**
         * @class NotificationBox
         * @brief This Class used to pop a notification box.
         *
         * NotificationBox provides functions to show and close notification box.
         */
        class NotificationBox::NotificationBoxImpl {
            public:
                void Show(const std::string& title, const std::string& message, const Color& color) {
                    msgColor = color;
                    barTitle = title;

                    enum CurrentDistibId hostType = CurrentDistibId::Ubuntu;
                    fibocom_get_current_distrib_id(&hostType);
                    if (notificationBox == nullptr) {
                        notificationBox = CreateProgressImpl(hostType);
                        if (notificationBox == nullptr) {
                            return;
                        }
                    }
                    notificationBox->fibocom_get_progress_environment_variable(notificationBox);
                    notificationBox->fibocom_set_progress_title(notificationBox, barTitle.c_str());
                    if (hostType == CurrentDistibId::Ubuntu) {
                        if (color == Color::RED) {
                            barMsg = "<span foreground='red' font='16'>" + message +"</span>";
                        } else if (color == Color::YELLOW) {
                            barMsg = "<span foreground='yellow' font='16'>" + message +"</span>";
                        } else {
                            barMsg = "<span foreground='black' font='16'>" + message +"</span>";
                        }
                    } else {
                        barMsg = message;
                    }
                    std::snprintf(notificationBox->progressText, sizeof(notificationBox->progressText), "%s", barMsg.c_str());  // NOLINT
                    //notificationBox->fibocom_set_progress_text(notificationBox, barMsg.c_str());
                    notificationBox->fibocom_set_progress_schedule(notificationBox, 0);  // NOLINT
                    notificationBox->fibocom_start_progress(notificationBox);
                }

                void Close() {
                    if (notificationBox == nullptr) {
                        return;
                    }

                    notificationBox->fibocom_close_progress(notificationBox);
                    DestroyProgressImpl(notificationBox);
                }
            private:
                Color msgColor = RED;
                std::string barMsg;
                std::string barTitle;
                Progress* notificationBox = nullptr;
        };

        NotificationBox::NotificationBox() : pImpl(std::make_unique<NotificationBoxImpl>()) {}
        NotificationBox::~NotificationBox() = default;

        void NotificationBox::Show(const std::string& title, const std::string& message, const Color& color) {
            pImpl->Show(title, message, color);
        }

        void NotificationBox::Close() {
            pImpl->Close();
        }

        std::string GetDisplayEnvironment()
        {
            enum CurrentDistibId hostType = CurrentDistibId::Ubuntu;
            fibocom_get_current_distrib_id(&hostType);

            Progress* progress = CreateProgressImpl(hostType);
            if (progress == nullptr) {
                return "";
            }

            progress->fibocom_get_progress_environment_variable(progress);
            std::string env(progress->environmentVariable);
            DestroyProgressImpl(progress);

            return env;
        }

         /**
         * @class FileExplorer
         * @brief This Class used to pop a file explorer.
         *
         * FileExplorer provides functions to show file explorer.
         */
        class FileExplorer::FileExplorerImpl {
            public:
                int Show(const std::string& path) {
                    std::string env = GetDisplayEnvironment();

                    // Check if the path is exist
                    struct stat buffer;
                    if (!(stat(path.c_str(), &buffer) == 0)) {
                        return 1; 
                    }
                    std::string fileExplorerCmd;
                    OSType os_type = GetOSType();
                    int ret = 0;
                    if (os_type == OSType::CROS) {
                        // Use Chrome OS native command to open the file
                        fileExplorerCmd = "chromeos-open \"" + path + "\"";
                    } else {
                        fileExplorerCmd = "su - $(logname) -c '" + env + "xdg-open \"" + path + "\" &> /dev/null &'";
                        std::cout << fileExplorerCmd << std::endl;
                        ret = std::system(fileExplorerCmd.c_str());
                    }

                    // Check if the xdg-open failed for path 
                    if (ret != 0) {
                        return 2; 
                    }

                    return 0;
                }

                void Close() {
                 /**
                 * @brief TODO This function no usefull.
                 */
                }
        };

        FileExplorer::FileExplorer() : pImpl(std::make_unique<FileExplorerImpl>()) {}
        FileExplorer::~FileExplorer() = default;

        int FileExplorer::Show(const std::string& path) {
            return  pImpl->Show(path);
        }

        void FileExplorer::Close() {
            pImpl->Close();
        }
    }
}