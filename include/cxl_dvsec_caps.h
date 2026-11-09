// -----------------------------------------------------------------------------
// cxl_dvsec_caps.h -- CXL-specific DVSEC capability descriptors.
//
//   CXL DVSECs use the PCIe Vendor-Specific Extended Capability header
//   (ID 000Bh) with DVSEC Vendor ID 1E98h and a DVSEC ID at +08h.  These
//   builders produce the standard CXL DVSEC structures that were not part of
//   the built-in Device (0000h) / Port (0003h) / Register Locator (0008h):
//
//     0002h  Non-CXL Function Map DVSEC
//     0004h  GPF DVSEC for CXL Port
//     0005h  GPF DVSEC for CXL Device
//     0007h  PCIe DVSEC for Flex Bus Port
//     0009h  MLD DVSEC (model profile)
//
//   Per-DUT profiles add them with CxlCfgSpace::apply_ext_caps(); the DVSEC
//   ID (not the offset) identifies them on the standard extended chain.
// -----------------------------------------------------------------------------
#pragma once

#include "pcie_ext_caps.h"

#include <cstdint>
#include <vector>

namespace cxl_tlm {
namespace cxl_dvsec {

constexpr std::uint16_t kVendorId      = 0x1E98;
constexpr std::uint16_t kFunctionMapId = 0x0002;
constexpr std::uint16_t kGpfPortId     = 0x0004;
constexpr std::uint16_t kGpfDeviceId   = 0x0005;
constexpr std::uint16_t kFlexBusPortId = 0x0007;
constexpr std::uint16_t kMldId         = 0x0009;

// Non-CXL Function Map: one 32-bit map (bit n = function n is non-CXL).
PcieExtCap function_map(std::uint32_t non_cxl_bitmap);

// GPF Port (DVSEC length 10h): Phase 1/2 Control 16-bit registers with
// timeout base [3:0] and scale [11:8].
PcieExtCap gpf_port(std::uint16_t phase1_control, std::uint16_t phase2_control);

// GPF Device (length 10h): Control RW, Status RO.
PcieExtCap gpf_device(std::uint16_t control);

// Flex Bus Port (length 10h): Capability RO, Control RW, Status RO.
PcieExtCap flexbus_port(std::uint16_t capability, std::uint16_t control_mask);

// MLD (model profile): version/num-LDs, LD ID list, per-LD memory size,
// VCS ID and flags.  The LD ID list is truncated to 16 entries.
PcieExtCap mld(std::uint16_t num_lds, const std::vector<std::uint16_t>& ld_ids,
               std::uint64_t ld_memory_size, std::uint16_t vcs_id,
               std::uint16_t flags);

} // namespace cxl_dvsec
} // namespace cxl_tlm
