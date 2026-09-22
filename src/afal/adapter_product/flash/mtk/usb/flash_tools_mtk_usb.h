#ifndef FIRMWARE_FLASH_LIB_SRC_FLASH_TOOLS_MTK_USB_H_
#define FIRMWARE_FLASH_LIB_SRC_FLASH_TOOLS_MTK_USB_H_

#include <memory>
#include <qchar.h>
#define _LINUX64

#include <optional>
#ifdef _IS_LINUX_
#include <unistd.h>
#endif

#include <string>

namespace afal {
	
namespace {

constexpr int K_BUFF = 512;
}

class MTKFlashLib {
  public:
    class Delegate {
      public:
        virtual ~Delegate() = default;
        Delegate() = default;
        Delegate(const Delegate&) = default;
        Delegate(Delegate&&) = delete;
        Delegate& operator=(const Delegate&) = default;
        Delegate& operator=(Delegate&&) = delete;
        virtual void UpdateProgress(int progress, const QString& messages) = 0;
    };
    MTKFlashLib() = default;
    MTKFlashLib(const MTKFlashLib&) = default;
    MTKFlashLib(MTKFlashLib&&) = default;
    MTKFlashLib& operator=(const MTKFlashLib&) = default;
    MTKFlashLib& operator=(MTKFlashLib&&) = default;
    virtual ~MTKFlashLib() = default;

    virtual bool
    Connect(std::optional<std::string> auth_file,
            std::optional<std::string> host_manifest_version = std::nullopt,
            std::optional<std::string> cert = std::nullopt) = 0;

    virtual void SetLogPath(const std::string & log_path) = 0;
    virtual bool EnterFlashMode(const std::string& flash_xml) = 0;

    virtual bool FlashAll(const std::string& xml_path,
                          const std::string& xml_name) = 0;

    virtual bool Flash(const std::string& pt_name,
                       const std::string& patch_path) = 0;

    virtual bool EraseAll() = 0;

    virtual bool Erase(int start_addr, int partition_size) = 0;

    virtual bool Read(const std::string& save_path, int start_addr, int partition_size) = 0;

    virtual bool Reboot() = 0;

    virtual bool IsConnect() = 0;

    virtual std::optional<std::string> GetDaInfo() = 0;

    static std::unique_ptr<MTKFlashLib> Create(Delegate *delegate);
};

} // namespace afal
#endif //FIRMWARE_FLASH_LIB_SRC_FLASH_TOOLS_MTK_USB_H_
