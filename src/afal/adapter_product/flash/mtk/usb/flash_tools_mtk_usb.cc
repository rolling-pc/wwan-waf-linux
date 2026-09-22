#include "flash_tools_mtk_usb.h"
#include "Flash.Mode.API.h"
#include "Flash.Mode.Struct.h"
#include "log.hpp"
#include <memory>
#include <optional>
#include <unistd.h>

#include <string>
#include <vector>

namespace afal {

namespace {

const std::string SYMBOLIC_CLUE = "COM://";
const std::string K_OPTIONS = "115200";
constexpr int K_SCAN_DEVICE_TIMEOUT = 1000 * 10;

using namespace log;

} // namespace

class MTKFlashLibImpl : public MTKFlashLib,
                        private callbacks_struct_t,
                        private callback_stop_t {
  public:
    MTKFlashLibImpl(const MTKFlashLibImpl&) = default;
    MTKFlashLibImpl(MTKFlashLibImpl&&) = default;
    MTKFlashLibImpl& operator=(const MTKFlashLibImpl&) = default;
    MTKFlashLibImpl& operator=(MTKFlashLibImpl&&) = default;
    MTKFlashLibImpl(Delegate* delegate);
    ~MTKFlashLibImpl() override;

    bool
    Connect(std::optional<std::string> auth_file,
            std::optional<std::string> host_manifest_version = std::nullopt,
            std::optional<std::string> cert = std::nullopt) override;

    bool EnterFlashMode(const std::string& flash_xml) override;

    bool FlashAll(const std::string& xml_path,
                  const std::string& xml_name) override;

    bool Flash(const std::string& pt_name,
               const std::string& patch_path) override;

    bool EraseAll() override;

    bool Erase(int start_addr, int partition_size) override;

    bool Read(const std::string& save_path, int start_addr,
              int partition_size) override;

    bool Reboot() override;

    bool IsConnect() override;

    void SetLogPath(const std::string& log_path) override;

    std::optional<std::string> GetDaInfo() override;

  private:
    int session{};
    bool connect{};
    Delegate* delegate;
    std::vector<char> symbolic_link;

    void OperationProgeess(unsigned int progress, unsigned long long data_xferd,
                           const char* info);
    bool NotifyStop();
    int SlaChallenge(const unsigned char* p_challenge_in,
                     unsigned int challenge_in_len,
                     unsigned char** pp_challenge_out,
                     unsigned int* p_challenge_out_len);
    int SlaChallengeEnd(unsigned char* p_challenge_out);
    bool EnterMode(const char* mode, const char* flash_xml_file_path,
                   const char* ui_config_xml_string);

    bool ExecuteCommand(const std::string& xml);
};

MTKFlashLibImpl::MTKFlashLibImpl(Delegate* delegate)
    : callbacks_struct_t(), callback_stop_t(), symbolic_link(K_BUFF),
      delegate(delegate) {
    trans._this = this;
    trans.cb_notify_stop = [](void* obj) -> bool {
        return reinterpret_cast<MTKFlashLibImpl*>(obj)->NotifyStop();
    };
    cb_notify_stop = trans.cb_notify_stop;
    trans.cb_op_progress =
        [](void* obj,
           unsigned int // NOLINT(bugprone-easily-swappable-parameters)
               progress,
           unsigned long long data_xferd, const char* info) -> void {
        reinterpret_cast<MTKFlashLibImpl*>(obj)->OperationProgeess(
            progress, data_xferd, info);
    };
    cb_sla.start_user_arg = this;
    cb_sla.end_user_arg = this;
    cb_sla.cb_start = [](void* obj, const unsigned char* p_challenge_in,
                         unsigned int challenge_in_len,
                         unsigned char** pp_challenge_out,
                         unsigned int* p_challenge_out_len) -> int {
        return reinterpret_cast<MTKFlashLibImpl*>(obj)->SlaChallenge(
            p_challenge_in, challenge_in_len, pp_challenge_out,
            p_challenge_out_len);
    };
    cb_sla.cb_end = [](void* obj, unsigned char* p_challenge_out) -> int {
        return reinterpret_cast<MTKFlashLibImpl*>(obj)->SlaChallengeEnd(
            p_challenge_out);
    };
}

MTKFlashLibImpl::~MTKFlashLibImpl() {
    if (session >= 0)
    {
        flashtool_destroy_session(session);
    }
    flashtool_cleanup();
}

void MTKFlashLibImpl::SetLogPath(const std::string& log_path) {
    logging_level_e log_level = logging_level_e::ktrace;
    flashtool_env_set_log(log_level, log_path.c_str());
}

bool MTKFlashLibImpl::IsConnect() {
    return connect;
};

bool MTKFlashLibImpl::Connect(std::optional<std::string> auth_file,
                              std::optional<std::string> host_manifest_version,
                              std::optional<std::string> cert) {
    ZHRESULT result = flashtool_startup(
        host_manifest_version ? (*host_manifest_version).c_str() : nullptr);
    if (result != E_OK)
    {
        FMTLOG_ERROR("Failed to startup flashtool environment");
        return false;
    }
    FMTLOG_DEBUG("startup falshtoo environment");

    session = flashtool_create_session();
    if (session == 0)
    {
        FMTLOG_ERROR("Failed to create session");
        return false;
    }

    FMTLOG_DEBUG("create session: {}", session);

    int timeout = K_SCAN_DEVICE_TIMEOUT;
    const char* options = "38400@";
    result = flashtool_scan_device(SYMBOLIC_CLUE.data(), symbolic_link.data(),
                                   int(symbolic_link.size()), timeout, this);
    if (result != E_OK)
    {
        FMTLOG_ERROR("Failed to scan device, error: {}",
                     flashtool_get_last_error_msg(session));
        return false;
    }
    FMTLOG_DEBUG("scan device:{}", symbolic_link.data());

    FMTLOG_DEBUG("auth_file:{}", (auth_file ? (*auth_file).c_str() : ""));
    result =
        flashtool_connect_device(session, symbolic_link.data(), options,
                                 auth_file ? (*auth_file).c_str() : nullptr,
                                 cert ? (*cert).c_str() : nullptr, this);
    if (result != E_OK)
    {
        FMTLOG_ERROR("Failed to connect to device, error: {}",
                     flashtool_get_last_error_msg(session));

        return false;
    }
    connect = true;
    return true;
}

std::optional<std::string> MTKFlashLibImpl::GetDaInfo() {
    std::vector<char> output_buffer(
        0x2000); // NOLINT(cppcoreguidelines-avoid-magic-numbers)
    // std::array<char, 100> formatted_string{};
    FMTLOG_DEBUG("target mem:{}", static_cast<void*>(output_buffer.data()));
    std::string xml;
    std::string formatted_string =
        fmt::format("<target_file>MEM://{}:0x2000</target_file>",
                    static_cast<void*>(output_buffer.data()));

    xml = fmt::format(R"(<?xml version="1.0" encoding="utf-8"?>)"
                      "<da>"
                      "<version>1.0</version>"
                      "<command>CMD:GET-DA-INFO</command>"
                      "<arg>"
                      "{}"
                      "</arg>"
                      "</da>",
                      formatted_string);
    if (!ExecuteCommand(xml))
    {
        return std::nullopt;
    }
    return output_buffer.data();
}

bool MTKFlashLibImpl::EnterMode(const char* mode,
                                const char* flash_xml_file_path,
                                const char* ui_config_xml_string) {
    FMTLOG_DEBUG(
        "mtk modem ready to enter {} mode, flash xml: {}, config xml: {}", mode,
        flash_xml_file_path, ui_config_xml_string);
    int result = flashtool_enter_mode(session, mode, flash_xml_file_path,
                                      ui_config_xml_string, &trans);
    if (result != E_OK)
    {
        FMTLOG_ERROR("Failed to enter flash mode, ret: {} error: {}", result,
                     flashtool_get_last_error_msg(session));
        return false;
    }
    return true;
}

bool MTKFlashLibImpl::ExecuteCommand(const std::string& xml) {
    FMTLOG_DEBUG("execute command:{}", xml);
    bool result = flashtool_execute_command(session, xml.c_str(), &trans);
    if (result != E_OK)
    {
        FMTLOG_ERROR("Failed to execute command, xml:{}, error:{}", xml,
                     flashtool_get_last_error_msg(session));
        return false;
    }
    return true;
}

bool MTKFlashLibImpl::EnterFlashMode(const std::string& flash_xml) {
    std::string command_template =
        R"(<?xml version="1.0" encoding="utf-8"?>)"
        "<da>"
        "<version>1.0</version>"
        "<command>CMD:SET-RUNTIME-PARAMETER</command>"
        "<arg>"
        "<checksum_level>NONE</checksum_level>"
        "<battery_exist>AUTO-DETECT</battery_exist>"
        "<da_log_level>INFO</da_log_level>"
        "<log_channel>UART</log_channel>"
        "<system_os>LINUX</system_os>"
        "</arg>"
        "</da>";
    return EnterMode("FLASH-MODE-DA", flash_xml.c_str(),
                     command_template.c_str());
}

bool MTKFlashLibImpl::FlashAll(const std::string& xml_path,
                               const std::string& xml_name) {
    std::string command_template =
        fmt::format(R"(<?xml version="1.0" encoding="utf-8"?>)"
                    "<da>"
                    "<version>1.0</version>"
                    "<command>CMD:FLASH-UPDATE</command>"
                    "<arg>"
                    "<path_separator>/</path_separator>"
                    "<backup_folder>"
                    "{}"
                    "</backup_folder>"
                    "<source_file>"
                    "{}"
                    "</source_file>"
                    "</arg>"
                    "</da>",
                    xml_path, xml_name);
    FMTLOG_DEBUG("Flash all arg: xml_path {}, xml_name {}", xml_path, xml_name);
    return ExecuteCommand(command_template);
}

bool MTKFlashLibImpl::Flash(const std::string& pt_name,
                            const std::string& patch_path) {
    std::string erase_partition_xml =
        fmt::format(R"(<?xml version="1.0" encoding="utf-8"?>)"
                    "<da>"
                    "<version>1.0</version>"
                    "<command>CMD:ERASE-PARTITION</command>"
                    "<arg>"
                    "<partition>"
                    "{}"
                    "</partition>"
                    "</arg>"
                    "</da>",
                    pt_name);
    std::string write_partition_xml =
        fmt::format(R"(<?xml version="1.0" encoding="utf-8"?>)"
                    "<da>"
                    "<version>1.0</version>"
                    "<command>CMD:WRITE-PARTITION</command>"
                    "<arg>"
                    "<partition>"
                    "{}"
                    "</partition>"
                    "<source_file>"
                    "{}"
                    "</source_file>"
                    "</arg>"
                    "</da>",
                    pt_name, patch_path);
    FMTLOG_DEBUG("Flash arg: pt_name {}, patch_path {}", pt_name, patch_path);
    if (!ExecuteCommand(erase_partition_xml))
    {
        FMTLOG_ERROR("erase {} failed", pt_name);
        return false;
    }

    if (!ExecuteCommand(write_partition_xml))
    {
        FMTLOG_ERROR("write {} failed", pt_name);
        return false;
    }
    return true;
}

bool MTKFlashLibImpl::Reboot() {
    std::string command_template2 = R"(<?xml version="1.0" encoding="utf-8"?>)"
                                    "<da>"
                                    "<version>1.0</version>"
                                    "<command>CMD:REBOOT</command>"
                                    "<arg>"
                                    "<action>IMMEDIATE</action>"
                                    "</arg>"
                                    "</da>";

    return ExecuteCommand(command_template2);
}

void MTKFlashLibImpl::OperationProgeess(unsigned int progress, // NOLINT
                                        unsigned long long data_xferd,
                                        const char* info) {
    // TODO Process the output of the mtk lib library to convert to overall progress
    FMTLOG_DEBUG("progress: {}, data: {}, info: {}", progress, data_xferd,
                 info);
}

bool MTKFlashLibImpl::NotifyStop() { // NOLINT
    return false;
}

int MTKFlashLibImpl::SlaChallenge(const unsigned char* p_challenge_in, // NOLINT
                                  unsigned int challenge_in_len, // NOLINT
                                  unsigned char** pp_challenge_out,
                                  unsigned int* p_challenge_out_len)// NOLINT 
{
    FMTLOG_DEBUG("challenge in: {}", (const char*)p_challenge_in);

    // 使用原始指针和长度代替 std::span
    unsigned int challenge_out_len_size = *p_challenge_out_len;
    for (unsigned int i = 0; i < challenge_out_len_size; ++i) {
        // 使用原始指针和长度模拟 std::span
        unsigned char* challenge_out = pp_challenge_out[i]; // NOLINT
        FMTLOG_DEBUG("challenge out: {}", (const char*)challenge_out);
    }
    
    return 0;
}
int MTKFlashLibImpl::SlaChallengeEnd(unsigned char* p_challenge_out) { // NOLINT
    FMTLOG_DEBUG("challenge out: {}", (const char*)p_challenge_out);
    return 0;
}

std::unique_ptr<MTKFlashLib> MTKFlashLib::Create(Delegate* delegate) {
    return std::make_unique<MTKFlashLibImpl>(delegate);
}

bool MTKFlashLibImpl::EraseAll() {
    std::string erase_all_xml =
        R"xxxy(<?xml version="1.0" encoding="utf-8"?>)xxxy"
        "<da>"
        "<version>1.0</version>"
        "<command>CMD:ERASE-FLASH</command>"
        "<arg>"
        "<partition>ALL</partition>"
        "<offset>0x0</offset>"
        "<length>MAX</length>"
        "</arg>"
        "</da>";

    return ExecuteCommand(erase_all_xml);
}

bool MTKFlashLibImpl::Erase(int start_addr, int partition_size) {
    std::string erase_partition_xml =
        fmt::format(R"xxxy(<?xml version="1.0" encoding="utf-8"?>)xxxy"
                    "<da>"
                    "<version>1.0</version>"
                    "<command>CMD:ERASE-FLASH</command>"
                    "<arg>"
                    "<partition>NAND-WHOLE</partition>"
                    "<offset>0x{:x}</offset>"
                    "<length>0x{:x}</length>"
                    "</arg>"
                    "</da>",
                    start_addr, partition_size);

    return ExecuteCommand(erase_partition_xml);
}

bool MTKFlashLibImpl::Read(const std::string& save_path, int start_addr,
                           int partition_size) {
    std::string read_xml = fmt::format(
        R"xml(<?xml version="1.0" encoding="utf-8"?><da>
        <version>1.0</version>
        <command>CMD:READ-FLASH</command>
        <arg>
            <partition>NAND-WHOLE</partition>
            <offset>0x{:x}</offset>
            <length>0x{:x}</length>
            <target_file>{}</target_file>
        </arg></da>)xml", start_addr, partition_size, save_path);

    return ExecuteCommand(read_xml);
}

} //namespace afal
