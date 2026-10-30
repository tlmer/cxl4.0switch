// -----------------------------------------------------------------------------
// cxl_cfg_space.h -- CXL configuration space (4 KB) with DVSEC structures.
//
//   Implements the M0.6 frozen map (model/cxl_reg_map.h):
//     - PCIe base: Type0 (device) or Type1 (root/USP/DSP) header, Command,
//       BAR sizing/programming and bridge windows.  This layer is delegated
//       to the complete PCIe model (pcie_cfg_space) through CxlPcieBase:
//       the CXL switch is a PCIe switch with CXL extensions.
//     - Standard capability chain (PM/MSI/PCIe/MSI-X) in spec order.
//     - PCIe DVSEC for CXL Devices (ID 0) or CXL Extensions DVSEC for Ports
//       (ID 3), Register Locator DVSEC (ID 8), and PCIe extended capabilities
//       (AER/ACS/ATS/PASID/SR-IOV/DPC/ReBAR).
//     - BAR0 (Component, 64 KB) and, for devices, BAR2 (CXL Device, 64 KB)
//
//   Access rules: 1/2/4-byte aligned accesses; RO writes ignored (counted);
//   out-of-range accesses counted as UR.  Reserved fields read 0.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_pcie_base.h"

#include <array>
#include <cstdint>
#include <vector>
#include <string>

namespace cxl_tlm {

enum class CxlCfgRole : std::uint8_t { DEVICE = 0, PORT = 1 };

struct CxlCfgSpace {
    CxlCfgRole role = CxlCfgRole::DEVICE;
    CxlPcieBase pcie;                 // complete PCIe base register layer
    std::array<std::uint8_t, 4096> raw{};

    // BAR state: index 0 = BAR0 (Component), 2 = BAR2 (Device, devices only).
    std::uint32_t bar_base[6] = {0, 0, 0, 0, 0, 0};
    bool          bar_probe[6] = {false, false, false, false, false, false};
    bool          bar_present[6] = {false, false, false, false, false, false};

    // Statistics (never silent).
    mutable std::uint64_t ur_count = 0;    // out-of-range config access
    std::uint64_t ro_write = 0;            // write to a read-only field
    mutable std::uint64_t unimpl_read = 0; // read of an unimplemented register
    std::uint64_t lock_block = 0;          // write blocked by CONFIG_LOCK
    mutable std::uint64_t align_err = 0;   // size/alignment violation

    void init(CxlCfgRole r);

    bool read(std::uint16_t off, unsigned size, std::uint32_t* val,
              std::string* err = nullptr) const;
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
    // PCIe capability-chain walkers (CXL switch = PCIe switch + CXL).
    std::vector<std::uint16_t> std_cap_ids() const;
    std::vector<std::uint16_t> ext_cap_ids() const;

    // DVSEC CXL Control/Status field helpers (device or port).
    std::uint16_t dvsec_control() const;
    bool set_dvsec_control(std::uint16_t v);      // false if locked
    bool config_locked() const;
    void set_viral_status(bool v);
    bool viral_status() const;

    std::uint64_t bar_window(int i) const { return pcie.bar_size(i); }
};

} // namespace cxl_tlm
