#include "firmware_flash_qc.h"
#include "proccommon.h"
namespace afal {

namespace {

using namespace log;

const int K_RECOVERY_TIMEOUT = 60;

const QString K_QDL_FLASH = "--storage";
const QString K_QDL_ARGUMENT = "nand";
const QString K_QDL_FW_PATH = "--include";
const int K_TIMEOUT = 300 * 1000;

bool RunQdl(ProcessWithOutput::Delegate* output_handler, const QString& fw_path,
            const QString& partition_table, const QString& firehose_agent,
            const QString& ext_xml) {
    QStringList arg;
    arg << K_QDL_FLASH;
    arg << K_QDL_ARGUMENT;
    arg << K_QDL_FW_PATH;
    arg << fw_path;
    arg << firehose_agent;
    arg << partition_table;
    arg << ext_xml;
    FMTLOG_DEBUG("QDL arguments: {}", arg);
    ProcessWithOutput process(QString(OS_PREFIX) + "/libs/adapter_product/tools/qc/qdl",
                              arg, K_TIMEOUT, output_handler);
    return process.Run() == 0;
}

}  // namespace

bool FirmwareFlashImplQcomUSB::RunFlashTools() {
    return RunQdl(this, fw_path, partition_table, firehose_agent, ext_xml);
}

bool FirmwareFlashImplQcomUSB::FastbootReboot() {
    if (!fastboot_device->FindDevice())
    {
        FMTLOG_ERROR("Not found fastboot device");
        return false;
    }
    return fastboot->Reboot();
}

bool FirmwareFlashImplQcomUSB::FastbootFlash(const QString& fw_path,
                                             const QString& fw_partition) {
    FMTLOG_DEBUG("Prepare for fastboot flash : {}, {}", fw_path, fw_partition);
    return fastboot->Flash(fw_partition, fw_path);
}

bool FirmwareFlashImplQcomUSB::FastbootErase(const QString& fw_partition) {
    FMTLOG_DEBUG("Enter Read, {}", fw_partition.toStdString());
    return fastboot->Erase(fw_partition);
}

}  // namespace afal