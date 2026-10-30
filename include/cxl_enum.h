// -----------------------------------------------------------------------------
// cxl_enum.h -- PCIe/CXL enumerator (M2.6/M2.8).
//
//   Walks bus 0 -> USP -> internal bus -> DSPs -> endpoints:
//     - programs Type1 bus numbers and BARs,
//     - for each model endpoint below a DSP: reads the Type0 header, probes and
//       programs BAR0, sets Command.MSE, walks the standard and extended
//       capability chains, and reads the CXL DVSEC / Register Locator / HDM,
//     - for the switch ports: reads the CXL Port DVSEC / Register Locator / HDM.
//   Devices below a DSP that have no model backend are the external customer EP
//   DUT (or TLM stub); they are NOT invented here -- each missing device is
//   recorded as a SKIP, never silently dropped.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_switch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlEnumDev {
    std::string path;
    std::uint8_t bus = 0, dev = 0, fn = 0;
    std::uint8_t sec = 0, sub = 0;              // Type1 downstream range
    std::uint16_t vid = 0, did = 0;
    std::uint8_t header_type = 0;
    std::uint32_t class_code = 0;
    std::uint64_t bar0_base = 0;
    std::uint16_t command = 0;                 // PCI Command (MSE bit 1)
    bool mse = false;
    int std_cap_count = 0;                     // standard PCIe cap chain
    std::vector<std::uint16_t> std_caps;       // cap IDs in chain order
    int ext_cap_count = 0;                     // PCIe extended cap chain
    std::vector<std::uint16_t> ext_caps;       // ext cap IDs in chain order
    bool has_cxl_dvsec = false;
    std::uint16_t dvsec_id = 0;
    bool has_regloc = false;
    int regloc_blocks = 0;
    int hdm_targets = 0;
    int mailbox_payload = 0;
    bool present = true;
};

struct CxlEnumConfig {
    std::uint8_t app_dev_num = 0;     // expected device number below a DSP
    std::uint64_t bar0_base = 0x80000000ull;
};

struct CxlDeviceTree {
    std::vector<CxlEnumDev> devs;
    bool ok = false;
    std::uint64_t skips = 0;
    std::string log;
};

CxlDeviceTree cxl_enumerate(CxlSwitch& sw, const CxlEnumConfig& cfg = CxlEnumConfig());
bool cxl_app_dev_num_ok(const CxlEnumConfig& cfg, std::uint8_t actual,
                        std::string* err = nullptr);

} // namespace cxl_tlm
