// -----------------------------------------------------------------------------
// cxl_ioctl.h -- ioctl-style control surface for the CXL Linux proxy.
//
//   The delivered library has no OS dependencies.  A character-device shim (or
//   a UVM test) calls cxl_model_ioctl() with the ABI in cxl_ioctl_ref.h; the
//   return value follows ioctl conventions (0 or a negative errno).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_ioctl_ref.h"
#include "cxl_linux.h"

#include <string>

namespace cxl_tlm {

int cxl_model_ioctl(CxlLinuxProxy* proxy, unsigned long request, void* arg,
                    std::string* err = nullptr);

} // namespace cxl_tlm
