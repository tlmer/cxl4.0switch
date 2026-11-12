// -----------------------------------------------------------------------------
// cxl_swdn_port.h -- model-side switch downstream port (M2.3).
//
//   One instance per customer swdn port.  Owns:
//     - Type1 configuration + CXL Extensions DVSEC for Ports + Register Locator
//     - DSP HDM decoder (model side; the CTL does not implement HDM)
//     - link state (run-time; the customer RTL owns the real link)
//
//   The port does NOT contain any CTL/EP functionality; it is the model-side
//   adapter endpoint (see md/ for the port adapter specification).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_cfg_space.h"
#include "cxl_hdm.h"

#include <cstdint>
#include <string>

namespace cxl_tlm {

struct CxlSwdnPort {
    int index = 0;
    CxlCfgSpace cfg;          // Type1 DSP
    CxlHdm hdm;               // DSP HDM (device target, 1 decoder)
    bool link_up = false;
    std::uint8_t bus = 0, sec = 0, sub = 0;
    std::uint16_t mem_base_reg = 0, mem_limit_reg = 0;       // 0x20
    std::uint16_t pref_base_reg = 0, pref_limit_reg = 0;     // 0x24
    std::uint16_t pref_base_hi = 0, pref_limit_hi = 0;       // 0x28 / 0x2C
    mutable std::uint64_t decode_hits = 0, decode_misses = 0;

    void init(int idx, std::uint8_t b = 0, std::uint8_t s = 0);
    void set_link(bool up) { link_up = up; }

    bool cfg_read(std::uint16_t off, unsigned size, std::uint32_t* v,
                  std::string* err = nullptr);
    bool cfg_write(std::uint16_t off, unsigned size, std::uint32_t val,
                   std::string* err = nullptr);

    bool program_hdm(std::uint64_t base, std::uint64_t size, std::string* err = nullptr);
    bool decode(std::uint64_t hpa, std::uint64_t* dpa, std::string* err = nullptr) const;

    // A memory request is decoded to this port if it falls in either the
    // non-prefetchable or the prefetchable Type-1 bridge window.
    bool in_mem_window(std::uint64_t addr) const;
    bool in_pref_window(std::uint64_t addr) const;
};

} // namespace cxl_tlm
