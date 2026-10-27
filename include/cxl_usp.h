// -----------------------------------------------------------------------------
// cxl_usp.h -- model CXL switch upstream port (M2.2 skeleton).
//
//   USP = Type1 configuration + CXL Extensions DVSEC for Ports + Register
//   Locator (via CxlCfgSpace), USP HDM decoder (CxlHdm, targets = N), and the
//   switch-level Mailbox/CCI (CxlMailbox).
//
//   Component register block offsets (BAR0): HDM at 0x5000, switch mailbox at
//   0xE000 (model placement for the switch-level CCI; see M0.6).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_cfg_space.h"
#include "cxl_hdm.h"
#include "cxl_mailbox.h"

#include <cstdint>
#include <string>

namespace cxl_tlm {

struct CxlUsp {
    CxlCfgSpace cfg;      // Type1 port
    CxlHdm hdm;           // USP decoder
    CxlMailbox mbx;       // switch-level mailbox

    std::uint8_t pbus = 0, sec = 0, sub = 0;
    // Type-1 window registers (PCIe encoding: 1 MB units in bits [15:4]).
    std::uint16_t mem_base_reg = 0, mem_limit_reg = 0;       // 0x20
    std::uint16_t pref_base_reg = 0, pref_limit_reg = 0;     // 0x24
    std::uint16_t pref_base_hi = 0, pref_limit_hi = 0;       // 0x28 / 0x2C

    void init(int target_ports);

    bool cfg_read(std::uint16_t off, unsigned size, std::uint32_t* v,
                  std::string* err = nullptr);
    bool cfg_write(std::uint16_t off, unsigned size, std::uint32_t val,
                   std::string* err = nullptr);

    // Component-register MMIO (relative to BAR0).
    bool mmio_read(std::uint32_t off, unsigned size, std::uint32_t* v,
                   std::string* err = nullptr);
    bool mmio_write(std::uint32_t off, unsigned size, std::uint32_t val,
                    std::string* err = nullptr);

    bool program_decoder(int n, std::uint64_t base, std::uint64_t size,
                         std::uint32_t target_port, std::string* err = nullptr);
    bool decode(std::uint64_t hpa, int* port, std::uint64_t* dpa,
                std::string* err = nullptr) const;

    bool in_mem_window(std::uint64_t addr) const;
    bool in_pref_window(std::uint64_t addr) const;
};

} // namespace cxl_tlm
