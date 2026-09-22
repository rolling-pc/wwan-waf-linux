#include "firmware_flash_qc_pcie.h"
#include "proccommon.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QProcess>
#include <QFile>
#include <QIODevice>

namespace afal {

namespace {

using namespace log;

const int K_RECOVERY_TIMEOUT = 60;
const int K_EDL_STABILIZE_DELAY_SEC = 3;
const QString K_SWITCH_PCIE_EDL_RW135R =
    "/sys/bus/pci/devices/0000:56:00.0/mhi0/trigger_edl";
const QString K_SWITCH_PCIE_EDL_RW151 =
    "/sys/bus/pci/devices/0000:74:00.0/mhi0/trigger_edl";

const QString K_QDL_FLASH = "--storage";
const QString K_QDL_ARGUMENT = "nand";
const QString K_QDL_FW_PATH = "--include";
const QString K_QDL_INTERFACE = "--interface";
const QString K_QDL_INTERFACE_ARG = "pcie";
const QString K_QDL_DEBUG = "--debug";
const int K_TIMEOUT = 300 * 1000;

bool RunQdl(ProcessWithOutput::Delegate* output_handler, const QString& fw_path,
            const QString& partition_table, const QString& firehose_agent,
            const QString& ext_xml) {
    (void)firehose_agent;
    QStringList arg;
    arg << K_QDL_INTERFACE;
    arg << K_QDL_INTERFACE_ARG;
    arg << K_QDL_DEBUG;
    arg << K_QDL_FLASH;
    arg << K_QDL_ARGUMENT;
    arg << K_QDL_FW_PATH;
    arg << fw_path;
    // arg << firehose_agent;
    arg << partition_table;
    arg << ext_xml;
    FMTLOG_DEBUG("QDL arguments: {}", arg);
    ProcessWithOutput process(QString(OS_PREFIX) + "/libs/adapter_product/tools/qc/qdl",
                              arg, K_TIMEOUT, output_handler);
    return process.Run() == 0;
}

}  // namespace

bool FirmwareFlashImplQcomPCIE ::RunFlashTools() {
    return RunQdl(this, fw_path, partition_table, firehose_agent, ext_xml);
}

// bool FirmwareFlashImplQcomPCIE ::FastbootReboot() {
//     if (!fastboot_device->FindDevice())
//     {
//         FMTLOG_ERROR("Not found fastboot device");
//         return false;
//     }
//     return fastboot->Reboot();
// }

// bool FirmwareFlashImplQcomPCIE ::FastbootFlash(const QString& fw_path,
//                                              const QString& fw_partition) {
//     FMTLOG_DEBUG("Prepare for fastboot flash : {}, {}", fw_path, fw_partition);
//     return fastboot->Flash(fw_partition, fw_path);
// }

// bool FirmwareFlashImplQcomPCIE ::FastbootErase(const QString& fw_partition) {
//     FMTLOG_DEBUG("Enter Read, {}", fw_partition.toStdString());
//     return fastboot->Erase(fw_partition);
// }

bool FirmwareFlashImplQcomPCIE::SwitchToEDLMode() {
    // ------------------------------------------------------------------
    // 1. Locate the "firehose" file in the download_agent directory
    // ------------------------------------------------------------------
    const QString search_dir = "/etc/opt/waf/waf_fw_pkg/FwPackage/download_agent";
    QDir dir(search_dir);
    if (!dir.exists()) {
        FMTLOG_ERROR("Search directory does not exist: {}", search_dir);
        return false;
    }

    QString src_file;
    const QStringList all_files = dir.entryList(QDir::Files);
    bool found = false;
    for (const QString& file_name : all_files) {
        if (file_name.contains("firehose", Qt::CaseInsensitive)) {
            src_file = dir.absoluteFilePath(file_name);
            found = true;
            FMTLOG_DEBUG("Found firehose file: {}", src_file);
            break;
        }
    }
    if (!found) {
        FMTLOG_ERROR("No firehose file found in {}", search_dir);
        return false;
    }

    // Verify the source file is readable
    const QFileInfo src_info(src_file);
    if (!src_info.isReadable()) {
        FMTLOG_ERROR("Firehose source file is not readable: {}", src_file);
        return false;
    }

    // ------------------------------------------------------------------
    // 2. Determine the destination directory based on modem type
    // ------------------------------------------------------------------
    QString dest_dir;
    if (modem_type == ModemType::QC_PCIE_RW151) {
        dest_dir = "/lib/firmware/qcom/sdx75/rolling/";
    } else {
        dest_dir = "/lib/firmware/qcom/sdx35/rolling/";
    }

    // Create the destination directory if it does not exist
    QDir dest_dir_obj(dest_dir);
    if (!dest_dir_obj.exists() && !dest_dir_obj.mkpath(".")) {
        FMTLOG_ERROR("Failed to create destination directory: {}", dest_dir);
        return false;
    }

    // ------------------------------------------------------------------
    // 3. Copy the firehose file into the destination directory
    // ------------------------------------------------------------------
    const QString dest_file = dest_dir + src_info.fileName();
    if (QFile::exists(dest_file)) {
        if (!QFile::remove(dest_file)) {
            FMTLOG_WARN("Failed to remove existing file: {}, copy may fail", dest_file);
        }
    }
    if (!QFile::copy(src_file, dest_file)) {
        FMTLOG_ERROR("Failed to copy firehose from {} to {}", src_file, dest_file);
        return false;
    }
    if (!QFile::setPermissions(dest_file,
                               QFile::ReadOwner | QFile::WriteOwner |
                               QFile::ReadGroup | QFile::ReadOther)) {
        FMTLOG_WARN("Failed to set permissions on {}", dest_file);
    }
    FMTLOG_DEBUG("Firehose file successfully copied to: {}", dest_file);

    // ------------------------------------------------------------------
    // 4. Resolve the PCIe device path:
    //    - Use lspci to obtain the short BDF (e.g. "02:00.0")
    //    - Match it against /sys/bus/pci/devices entries to get the
    //      full directory name (e.g. "0000:02:00.0")
    // ------------------------------------------------------------------
    QString device_id;
    if (modem_type == ModemType::QC_PCIE_RW151) {
        device_id = "17cb:0309";
    } else {
        device_id = "17cb:011a";
    }

    // 4.1 Run "lspci -d <device_id>" to obtain the short BDF
    QProcess lspci_proc;
    lspci_proc.start("lspci", QStringList() << "-d" << device_id);
    if (!lspci_proc.waitForFinished(3000)) {
        FMTLOG_ERROR("lspci command timed out for device {}", device_id);
        return false;
    }
    if (lspci_proc.exitCode() != 0) {
        FMTLOG_ERROR("lspci command failed with exit code {}", lspci_proc.exitCode());
        return false;
    }

    const QString output =
        QString::fromUtf8(lspci_proc.readAllStandardOutput()).trimmed();
    if (output.isEmpty()) {
        FMTLOG_ERROR("No PCIe device found with ID {}", device_id);
        return false;
    }

    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        FMTLOG_ERROR("Unexpected empty output from lspci");
        return false;
    }

    const QString first_line = lines.first().trimmed();
    const QStringList tokens = first_line.split(' ', Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        FMTLOG_ERROR("Failed to parse lspci output: {}", first_line);
        return false;
    }

    // Short BDF, e.g. "02:00.0"
    const QString short_bdf = tokens.first();
    FMTLOG_DEBUG("lspci reported short BDF: {}", short_bdf);

    // 4.2 Walk /sys/bus/pci/devices and match the entry whose name ends
    //     with the short BDF (sysfs uses the full form "DDDD:BB:SS.F").
    //     Use endsWith() instead of contains()/strstr() to avoid false
    //     matches such as "02:00.0" matching "0000:00:02.0".
    const QString pci_devices_dir = "/sys/bus/pci/devices";
    QDir pci_dir(pci_devices_dir);
    if (!pci_dir.exists()) {
        FMTLOG_ERROR("PCI sysfs directory does not exist: {}", pci_devices_dir);
        return false;
    }

    QString full_bdf;  // e.g. "0000:02:00.0"
    const QStringList pci_entries =
        pci_dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : pci_entries) {
        if (entry.endsWith(short_bdf, Qt::CaseInsensitive)) {
            full_bdf = entry;
            FMTLOG_DEBUG("Matched sysfs PCI entry: {} (short BDF {})",
                         entry, short_bdf);
            break;
        }
    }

    if (full_bdf.isEmpty()) {
        FMTLOG_ERROR("No sysfs PCI entry found matching BDF {} under {}",
                     short_bdf, pci_devices_dir);
        return false;
    }

    // 4.3 Build the trigger_edl path using the full BDF
    const QString edl_path =
        QString("/sys/bus/pci/devices/%1/mhi0/trigger_edl").arg(full_bdf);
    FMTLOG_DEBUG("EDL trigger path: {}", edl_path);

    if (!QFile::exists(edl_path)) {
        FMTLOG_ERROR("trigger_edl file does not exist: {}", edl_path);
        return false;
    }

    // ------------------------------------------------------------------
    // 5. Trigger EDL mode by writing "1" to the sysfs node
    // ------------------------------------------------------------------
    QFile edl_trigger(edl_path);
    if (!edl_trigger.open(QIODevice::WriteOnly)) {
        FMTLOG_ERROR("Failed to open trigger_edl: {}", edl_path);
        return false;
    }
    const qint64 written = edl_trigger.write("1");
    edl_trigger.close();
    if (written != 1) {
        FMTLOG_ERROR("Failed to write '1' to trigger_edl (written={})", written);
        return false;
    }

    // ------------------------------------------------------------------
    // 6. Poll for /dev/wwan0firehose0 (up to 15 attempts, 2s interval)
    // ------------------------------------------------------------------
    const QString device_path = "/dev/wwan0firehose0";
    const int max_attempts = 18;
    const int sleep_sec = 2;
    bool device_found = false;

    FMTLOG_DEBUG("Waiting for EDL device file: {}", device_path);
    for (int i = 0; i < max_attempts; ++i) {
        if (QFile::exists(device_path)) {
            device_found = true;
            FMTLOG_DEBUG("EDL device found after {} attempts", i + 1);
            break;
        }
        FMTLOG_DEBUG("Attempt {}/{}: device not found, sleeping {}s",
                     i + 1, max_attempts, sleep_sec);
        std::this_thread::sleep_for(std::chrono::seconds(sleep_sec));
    }

    if (!device_found) {
        FMTLOG_ERROR("EDL device file {} not found after {} seconds",
                     device_path, max_attempts * sleep_sec);
        return false;
    }

    return true;
}

bool FirmwareFlashImplQcomPCIE ::EdlErase() {
    FMTLOG_DEBUG("Edl Erase, QDL Erase");

    if (partition_table.isEmpty() || firehose_agent.isEmpty())
    {
        FMTLOG_ERROR("Config is failed, Edl flash failed!");
        return false;
    }

    if (!WaitEdlDevice())
    {
        return false;
    }

    if (!RunFlashTools())
    {
        FMTLOG_ERROR("Qdl run failed");
        return false;
    }

    return true;
}

// bool FirmwareFlashImplQcomPCIE::RunQBhiServer(const QString& args)
// {
//     FMTLOG_ERROR("This feature is not supported yet.");
//     return false;
// }

}  // namespace afal
