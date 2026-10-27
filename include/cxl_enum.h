// -----------------------------------------------------------------------------
// cxl_enum.h -- lightweight CXL enumerator (M2.6).
//
//   Walks bus 0 -> USP -> internal bus -> DSPs, programs bus numbers and BAR0,
//   reads CXL DVSEC / Register Locator / HDM / mailbox capabilities, and
//   produces a device tree.  Devices below a DSP are the customer EP DUT (or
//   the TLM stub); they are NOT invented here -- each missing device is
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
    std::uint16_t vid = 0, did = 0;
    std::uint8_t header_type = 0;
    std::uint32_t class_code = 0;
    std::uint64_t bar0_base = 0;
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
