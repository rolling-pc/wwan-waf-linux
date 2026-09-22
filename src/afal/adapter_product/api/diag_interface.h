#ifndef DIAG_INTERFACE_H_
#define DIAG_INTERFACE_H_

#include "common.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace PA {

class Diag {
public:
  class Delegate {
  public:
    virtual ~Delegate() = default;
  };

  using DiagCommand = std::vector<char>;

  virtual ~Diag() = default;

  virtual bool Enable() = 0;

  virtual bool Disable() = 0;

  virtual std::optional<DiagCommand> Command(const DiagCommand &command ) = 0;

  // Create a Diag object
  std::unique_ptr<Diag> Create(ModemType modem_type = ModemType::QCOM_USB_MODEM,
                               std::weak_ptr<Diag::Delegate> delegate =
                                   std::make_shared<Diag::Delegate>());

private:
  std::weak_ptr<Delegate> *delegate_;
};

using d = Diag::Delegate;

} // namespace PA

#endif // DIAG_INTERFACE_H_
