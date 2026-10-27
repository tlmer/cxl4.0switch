// -----------------------------------------------------------------------------
// cxl_cfg_space.h -- CXL configuration space (4 KB) with DVSEC structures.
//
//   Implements the M0.6 frozen map (model/cxl_reg_map.h):
//     - Type0 (CXL device) or Type1 (root/USP/DSP) standard header
//     - PCIe DVSEC for CXL Devices (ID 0) or CXL Extensions DVSEC for Ports (ID 3)
//     - Register Locator DVSEC (ID 8) with Component + Device register blocks
//     - BAR0 (Component, 64 KB) and, for devices, BAR2 (CXL Device, 64 KB)
//
//   Access rules: 1/2/4-byte aligned accesses; RO writes ignored (counted);
//   out-of-range accesses counted as UR.  Reserved fields read 0.
// -----------------------------------------------------------------------------
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace cxl_tlm {

enum class CxlCfgRole : std::uint8_t { DEVICE = 0, PORT = 1 };

struct CxlCfgSpace {
    CxlCfgRole role = CxlCfgRole::DEVICE;
    std::array<std::uint8_t, 4096> raw{};

    // BAR state: index 0 = BAR0 (Component), 2 = BAR2 (Device, devices only).
    std::uint32_t bar_base[6] = {0, 0, 0, 0, 0, 0};
    bool          bar_probe[6] = {false, false, false, false, false, false};
    bool          bar_present[6] = {false, false, false, false, false, false};

    // Statistics (never silent).
    std::uint64_t ur_count = 0;        // out-of-range config access
    std::uint64_t ro_write = 0;        // write to a read-only field
    std::uint64_t unimpl_read = 0;     // read of an unimplemented register
    std::uint64_t lock_block = 0;      // write blocked by CONFIG_LOCK
    std::uint64_t align_err = 0;       // size/alignment violation

    void init(CxlCfgRole r);

    bool read(std::uint16_t off, unsigned size, std::uint32_t* val,
              std::string* err = nullptr);
    bool write(std::uint16_t off, unsigned size, std::uint32_t val,
               std::string* err = nullptr);

    // Helpers used by models/tests.
    std::uint16_t rd16(std::uint16_t off) const;
    std::uint32_t rd32(std::uint16_t off) const;
    void wr16(std::uint16_t off, std::uint16_t v);
    void wr32(std::uint16_t off, std::uint32_t v);

    std::uint16_t dvsec_offset() const;           // device or port DVSEC
    std::uint16_t regloc_offset() const { return 0x300; }
    int regloc_entries() const;                   // 2 device / 1 port
    std::uint16_t regloc_entry(int i) const;      // Offset Low address
    std::uint16_t find_ext_cap(std::uint16_t cap_id) const;

    // DVSEC CXL Control/Status field helpers (device or port).
    std::uint16_t dvsec_control() const;
    bool set_dvsec_control(std::uint16_t v);      // false if locked
    bool config_locked() const;
    void set_viral_status(bool v);
    bool viral_status() const;

    std::uint64_t bar_window(int i) const { return bar_present[i] ? 0x10000ull : 0ull; }
};

} // namespace cxl_tlm
