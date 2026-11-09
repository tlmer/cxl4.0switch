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
#include "cxl_aer.h"
#include "cxl_pcie_base.h"
#include "cxl_reg_map.h"
#include "pcie_ext_caps.h"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include <string>

namespace cxl_tlm {

struct CxlDoeDevice;   // model/cxl_doe.h -- device-side DOE responder

// DEVICE/PORT are the legacy roles kept for existing register tests.  The
// standard hierarchy uses HOST_BRIDGE / ROOT_PORT / UPSTREAM_PORT /
// DOWNSTREAM_PORT / ENDPOINT.  Header type, class code and the PCIe
// capability type are derived from the role (see CxlCfgSpace::init).
enum class CxlCfgRole : std::uint8_t {
    DEVICE = 0,
    PORT = 1,             // legacy downstream port (PCIe cap type 6)
    HOST_BRIDGE = 2,
    ROOT_PORT = 3,
    UPSTREAM_PORT = 4,
    DOWNSTREAM_PORT = 5,
    ENDPOINT = 6
};

inline bool cxl_role_is_endpoint(CxlCfgRole r) {
    return r == CxlCfgRole::DEVICE || r == CxlCfgRole::ENDPOINT;
}
inline bool cxl_role_is_host_bridge(CxlCfgRole r) {
    return r == CxlCfgRole::HOST_BRIDGE;
}
inline bool cxl_role_is_bridge(CxlCfgRole r) {
    return r == CxlCfgRole::PORT || r == CxlCfgRole::ROOT_PORT ||
           r == CxlCfgRole::UPSTREAM_PORT || r == CxlCfgRole::DOWNSTREAM_PORT;
}

struct CxlCfgSpace {
    CxlCfgRole role = CxlCfgRole::DEVICE;
    std::uint8_t dvsec_rev_override = 0xFFu;   // 0xFF = model default
    CxlPcieBase pcie;                 // complete PCIe base register layer
    std::array<std::uint8_t, 4096> raw{};

    // BAR state: index 0 = BAR0 (Component), 2 = BAR2 (Device, devices only).
    std::uint32_t bar_base[6] = {0, 0, 0, 0, 0, 0};
    bool          bar_probe[6] = {false, false, false, false, false, false};
    bool          bar_present[6] = {false, false, false, false, false, false};

    // R5: ReBAR face @0x3C0 (standard Linux pci_regs.h layout).
    // Supported sizes bitmap (bit N => 1MB << N) and the selected N.  The
    // reset BAR0 is 64 KB, which is below the ReBAR minimum; the selected
    // field starts at N=0 and the first accepted control write resizes BAR0.
    std::uint32_t rebar_sizes = 0x15u;   // 1 MB (0), 4 MB (2), 16 MB (4)
    std::uint8_t  rebar_sel = 0;
    std::uint64_t rebar_gated_block = 0; // resize attempts blocked by MSE

    // Optional CPMU Register Locator instance (CXL 4.0 8.2.7 / block ID 04h).
    // Disabled by default so the legacy two-entry device RegLoc layout and all
    // existing tests are unchanged.  When enabled, the endpoint RegLoc gains a
    // third entry (BIR 2, block 04h) at cpmu_offset; the caller maps the block
    // through CxlCpmu/CxlScComponent.
    bool          cpmu_present = false;
    std::uint32_t cpmu_bir = 2;
    std::uint32_t cpmu_offset = 0x10000;
    // Additional CPMU register blocks (same block ID 04h) at further 64 KB-
    // aligned offsets.  Opt-in; each block keeps cpmu_bir.  The Register
    // Locator gains one entry per block and BAR2 grows by 64 KB per block.
    std::vector<std::uint32_t> cpmu_extra_offsets;
    int cpmu_block_count() const
    {
        return cpmu_present ? (int)(1 + cpmu_extra_offsets.size()) : 0;
    }

    // Optional PCIe VPD capability (ID 03h) at 0xC0.  The model exposes a
    // read-only VPD area (identifier string + VPD-R) through the standard
    // Address/Data register window; default off keeps the legacy cap chain.
    bool          vpd_present = false;
    mutable std::uint16_t vpd_addr = 0;   // auto-increments on data-register reads
    bool          vpd_write_flag = false;
    std::vector<std::uint8_t> vpd_data;
    std::uint64_t vpd_writes = 0;      // writes to the read-only VPD area

    // Statistics (never silent).
    mutable std::uint64_t ur_count = 0;    // out-of-range config access
    std::uint64_t ro_write = 0;            // write to a read-only field
    mutable std::uint64_t unimpl_read = 0; // read of an unimplemented register
    std::uint64_t lock_block = 0;          // write blocked by CONFIG_LOCK
    mutable std::uint64_t align_err = 0;   // size/alignment violation

    void init(CxlCfgRole r);

    // Role-derived identity (Linux/lspci observable).
    std::uint8_t header_type() const;
    std::uint32_t class_code() const;
    std::uint8_t pcie_cap_type() const;

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
    // DVSEC Revision profile (CXL revisions are mirrored by DUT profiles).
    // The default is the model's device/port revision; a profile can select
    // another revision to match a CXL 1.1/2.0/3.x/4.0 DUT.
    void set_dvsec_revision(std::uint8_t rev);
    std::uint8_t dvsec_revision() const;
    std::uint16_t regloc_offset() const { return kCfgDvsecRegLocOff; }
    int regloc_entries() const;                   // 2 device / 1 port (+1 CPMU)
    std::uint16_t regloc_entry(int i) const;      // Offset Low address
    std::uint16_t find_ext_cap(std::uint16_t cap_id) const;

    // ---- Standard PCIe Multicast extended capability (ID 0x0012) ----------
    // Enabled for switch ports (USP and DSPs).  The register block is the
    // PCIe r4.0 sec 7.9.11 layout; see cxl_cfg_space.cpp for the fields.
    // A switch/Root Complex port must implement MC_Overlay_BAR; an Endpoint
    // must not, which is why mc_present is false for endpoint roles.
    static constexpr std::uint16_t kMcCapOff = 0x3E0;
    bool mc_present = false;
    std::uint64_t mc_reject = 0;     // invalid MC control writes (never silent)
    std::function<void()> on_mc_update;   // routing sync hook (CxlScSwitch)

    bool     mc_enabled() const;
    std::uint32_t mc_max_groups() const;       // encoded value + 1
    std::uint32_t mc_num_groups() const;       // encoded value + 1
    std::uint32_t mc_index() const;            // MC_Index_Position
    std::uint64_t mc_base() const;             // MC_Base_Address (4KB aligned)
    std::uint64_t mc_receive() const;
    std::uint64_t mc_block_all() const;
    std::uint64_t mc_block_untranslated() const;
    std::uint32_t mc_overlay_size() const;     // < 6 => overlay disabled
    std::uint64_t mc_overlay_base() const;

    // Program the window/control fields through the same write path software
    // uses (registers and routing stay consistent).
    void set_mc(bool enable, std::uint64_t base, std::uint32_t index,
                std::uint32_t num_groups);
    void set_mc_receive(std::uint64_t bitmap);
    void set_mc_block(std::uint64_t all, std::uint64_t untranslated);
    void set_mc_overlay(std::uint32_t size, std::uint64_t base);

    // ---- Per-DUT standard extended capability profile --------------------
    // A customer DUT exposes its own capability set; the model lays the
    // selected descriptors out after the built-in CXL capabilities and links
    // the chain (see model/pcie_ext_caps.h).  add_ext_cap returns the
    // assigned offset, or 0 when the role has no extended chain / no room.
    static constexpr std::uint16_t kExtraCapBase = 0x420;
    std::vector<PcieExtCap> extra_caps;
    std::uint16_t ext_chain_tail_ = 0;   // last extended cap in the raw chain
    std::uint16_t add_ext_cap(const PcieExtCap& cap);
    void apply_ext_caps(const std::vector<PcieExtCap>& caps);
    // Called after a profile extended-capability register write:
    // (capability ID, offset relative to the capability, access size, value,
    //  live descriptor so data-plane state can latch tables).
    std::function<void(std::uint16_t, std::uint16_t, unsigned, std::uint32_t,
                       const PcieExtCap&)>
        ext_cap_write_hook;

    // Built-in capability selection (per-DUT profiles).  A 64-bit mask keyed
    // by standard capability ID gates AER (0x0001), Multicast (0x0012),
    // ACS (0x000D), ATS (0x000F), SR-IOV (0x0010), ReBAR (0x0015),
    // PASID (0x001B) and DPC (0x001D).  Default 0 = all present (current
    // behavior).  The CXL DVSEC and Register Locator are mandatory and are
    // not gateable.  Call rebuild_builtin_chain() after changing the mask.
    std::uint64_t builtin_cap_disable = 0;
    void set_builtin_cap_enabled(std::uint16_t id, bool enabled);
    bool builtin_cap_enabled(std::uint16_t id) const;
    void rebuild_builtin_chain();

    // Optional live AER model (status/mask/severity/header log + injection).
    // When attached, the AER registers at 0x304..0x32B are served from it.
    CxlAer* aer = nullptr;

    // Optional device-side DOE responder (model/cxl_doe.h).  The standard
    // DOECAP mailbox registers at doe_off are served from the responder:
    //   +0x08 Control ([0] Abort, [1] Interrupt Enable, [31] Go)
    //   +0x0C Status  ([0] Busy, [2] Error, [31] Data Object Ready)
    //   +0x10 Write Data mailbox, +0x14 Read Data mailbox
    CxlDoeDevice* doe = nullptr;
    std::uint16_t doe_off = 0;
    std::uint32_t doe_ctl = 0;       // standard Control bits [1:0]
    void attach_doe(CxlDoeDevice* d, std::uint16_t cap_off);
    // PCIe capability-chain walkers (CXL switch = PCIe switch + CXL).
    std::vector<std::uint16_t> std_cap_ids() const;
    std::vector<std::uint16_t> ext_cap_ids() const;

    // DVSEC CXL Control/Status field helpers (device or port).
    std::uint16_t dvsec_control() const;
    bool set_dvsec_control(std::uint16_t v);      // false if locked
    bool config_locked() const;
    void set_viral_status(bool v);
    bool viral_status() const;

    // CXL 2.0 Device DVSEC memory ranges (Tables 8-17..8-24).  Range size and
    // base are 4 GB granular (32-bit registers) with the low register carrying
    // size/base bits [31:28] plus Info Valid [0] and Active [1].  The device
    // sets Info Valid when a range is programmed and Active when Mem_Enable
    // (DVSEC Control bit 2) is on.
    void set_mem_range(int idx, std::uint64_t base, std::uint64_t size);
    bool mem_range(int idx, std::uint64_t* base, std::uint64_t* size) const;
    bool mem_range_info_valid(int idx) const;
    bool mem_range_active(int idx) const;

    std::uint64_t bar_window(int i) const { return pcie.bar_size(i); }
};

} // namespace cxl_tlm
