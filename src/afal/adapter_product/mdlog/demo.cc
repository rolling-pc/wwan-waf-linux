#include <memory>
#include <qcoreapplication.h>
#include <qfileinfo.h>
#include <qtimer.h>
#include "log.hpp"
#include "common/modem.h"

#include "modem_log_interface.h"

using namespace afal::log;

int main(int argc, char** argv) {

    afal::ModemLoggerType logger_create_qc{};
    afal::ModemLoggerType logger_create_mtk{};

    auto app = std::make_unique<QCoreApplication>(argc, argv);
    Logger::Instance().Init(CONSOLE | CUSTOM, "ModemLogDemo");
    Logger::Instance().SetLevel(DEBUG);
    FMTLOG_DEBUG("test");

    QFileInfo file_info_qc("mdlog_qc.dll");
    if (file_info_qc.exists()) {
        logger_create_qc = afal::LoadMdLogCreateFunc(file_info_qc.absoluteFilePath());
    }
    else
        FMTLOG_ERROR("error #$#######");


    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(50000); // NOLINT 100s
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    FMTLOG_INFO("RW135_CAPTURE_MDLOG test runing");
    if (!logger_create_qc) {
        FMTLOG_ERROR("logger create function load failed!, logger obj could not be created.");
        return 1;
    }
    std::unique_ptr<afal::ModemLogger> logger(logger_create_qc(afal::ModemType::QC_USB_GC));
    logger->SetConfig(afal::ModemLogger::Config("COM13", "D:/FBC/QTrace", 1, "full.cfg", 100)); // NOLINT
    if (!logger->Enable()) {
        FMTLOG_ERROR("Modem Debug enable failed.");
    }
    if (!logger->Status()) {
        FMTLOG_ERROR("Modem logger status failed.");
    }

    FMTLOG_INFO("Modem logger will start.");
    auto ret = logger->Start();
    if (!ret) {
        FMTLOG_ERROR("Modem logger start failed.");
        return 1;
    }

    FMTLOG_INFO("Modem logger  start ok will get log info.");
    if (ret) {
        loop.exec();
    }

    FMTLOG_INFO("Modem logger   will stop loop is exit!.");
    if (!logger->Stop()) {
        FMTLOG_ERROR("Modem logger stop failed.");
    }

    FMTLOG_INFO("Modem logger    stop ok,will disable!.");
    if (!logger->Disable()) {
        FMTLOG_ERROR("Modem logger disable failed.");
    }

    FMTLOG_INFO("Modem logger    disable ok,will get logger status!.");
    if (logger->Status()) {
        FMTLOG_ERROR("Modem logger status check failed.");
    }

    FMTLOG_INFO("Modem logger  get logger statu ok,will return.");

    return app->exec();
}