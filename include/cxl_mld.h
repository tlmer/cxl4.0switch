// -----------------------------------------------------------------------------
// cxl_mld.h -- P2 MLD/FMLD + virtual CXL switch (vCS) minimal model.
//
//   An MLD exposes multiple Logical Devices (LDs), each with its own DPA
//   space.  A vCS is a virtual switch instance with vPPBs; binding a vPPB to an
//   MLD LD gives one host a private view of that LD.  Multiple vCSs may share
//   one physical MLD port, each bound to a different LD (CXL 4.0 Section 7.1.3).
//
//   vPPB PCIe configuration space: full Type 1 header + PM/PCIe/Slot/MSI
//   capability chain at the standard offsets (0x40/0x50/0x74) and slot power
//   sequencing with Command Completed / presence-change status.  The MLD
//   DVSEC surface covers the implemented fields only; unimplemented offsets
//   read as zero (documented boundary).
// -----------------------------------------------------------------------------
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cxl_tlm {

enum class CxlVppbPowerState : std::uint8_t { OFF = 0, RAMPING = 1, ON = 2 };

// F3: configurable board/rail slot power timing.  Each rail becomes good
// delay_ticks + ramp_ticks + power_good_ticks after the power-on request;
// PERST# is released reset_ticks after the last rail is good.
struct CxlVppbPowerRail {
    std::string name;
    std::uint32_t delay_ticks = 0;
    std::uint32_t ramp_ticks = 0;
    std::uint32_t power_good_ticks = 0;
    std::uint32_t good_at() const { return delay_ticks + ramp_ticks + power_good_ticks; }
};

struct CxlVppbConfig {
    // ---- PCI Type 1 configuration header -----------------------------------
    std::uint16_t vendor_id = 0x1E98, device_id = 0x0002;
    std::uint8_t revision = 1, class_code = 0x06, subclass = 0x07;
    std::uint8_t cache_line_size = 0, latency_timer = 0;
    std::uint16_t command = 0x0002;      // MEM space enable
    std::uint16_t status = 0x0010;       // capabilities list
    std::uint32_t bar0 = 0, bar1 = 0;
    std::uint8_t pbus = 0, secbus = 1, subbus = 0xFF, sec_latency = 0;
    std::uint16_t io_base = 0x00F0, io_limit = 0x0000;
    std::uint16_t secondary_status = 0;
    std::uint16_t mem_base = 0xFFF0, mem_limit = 0x0000;
    std::uint16_t pref_base = 0xFFF1, pref_limit = 0x0001;
    std::uint32_t pref_base_hi = 0, pref_limit_hi = 0;
    std::uint16_t io_base_hi = 0, io_limit_hi = 0;
    std::uint32_t rom_bar = 0;           // [0] enable, [31:11] base
    std::uint8_t int_line = 0, int_pin = 1;
    std::uint16_t bridge_ctrl = 0;

    // ---- Capability chain: PM @0x40 -> PCIe @0x50 -> MSI @0x74 ------------
    // PM capability (8 bytes).
    std::uint32_t pm_cap = 0x00005001;   // CapID 0x01, next 0x50
    std::uint16_t pm_pmc = 0x4003;       // version 3, PME from D3hot
    std::uint16_t pmcsr = 0;             // D0, PME disabled
    std::uint8_t pmcsr_bse = 0, pm_data = 0;
    std::uint64_t pme_events = 0, pme_denied = 0, pm_invalid_writes = 0;

    // PCIe capability (v1, Root Port, slot implemented).  Register offsets
    // follow pci_regs.h: devcap +0x04, devctl +0x08, devsta +0x0A,
    // linkcap +0x0C, linkctl +0x10, linksta +0x12, slotcap +0x14,
    // slotctl +0x18, slotsta +0x1A, rootctl +0x1C, rootcap +0x1E,
    // rootsta +0x20.
    std::uint32_t pcie_devcap = 0x00008022;   // MPS 512B, ext tag, RBER
    std::uint16_t pcie_devctl = 0;
    std::uint16_t pcie_devsta = 0;
    std::uint32_t link_cap = 0x00100D04;      // 16 GT/s, x16, ASPM L0s/L1, DLLLA
    std::uint16_t link_ctl = 0;
    std::uint16_t link_status = 0x3104;       // 16 GT/s, x16, SLC, DLL active
    std::uint32_t slot_cap = 0x0000007F;      // ABP/PCP/MRL/AI/PI/HPS/HPC
    std::uint16_t slot_ctrl = 0;
    std::uint16_t slot_status = 0;            // sticky event bits
    std::uint16_t root_ctl = 0, root_cap = 0x0001;
    std::uint32_t root_status = 0;

    // MSI capability (64-bit, per-vector masking, one vector).
    std::uint32_t msi_cap = 0x00000005;       // CapID 0x05, no next
    std::uint16_t msi_ctrl = 0x0180;          // 64-bit + per-vector mask
    std::uint32_t msi_addr_lo = 0, msi_addr_hi = 0, msi_data = 0;
    std::uint32_t msi_mask = 0, msi_pending = 0;

    // ---- F2: PCIe extended capability surface (AER/ATS/ACS/SR-IOV) --------
    // AER @0x100, ATS @0x140, ACS @0x150, SR-IOV @0x160.
    std::uint32_t aer_uc_status = 0, aer_uc_mask = 0, aer_uc_severity = 0;
    std::uint32_t aer_c_status = 0, aer_c_mask = 0;
    std::uint32_t aer_cap_ctl = 0;            // bit0 ECRC gen, bit1 ECRC check
    std::uint32_t aer_hdr_log[4] = {0, 0, 0, 0};
    std::uint32_t aer_root_cmd = 0, aer_root_status = 0, aer_err_source = 0;
    std::uint64_t aer_injected = 0;
    std::uint16_t ats_ctl = 0;                // bit0 enable, [7:1] STU
    std::uint64_t ats_inval_req = 0, ats_inval_cpl = 0;
    std::uint16_t acs_ctl = 0;                // V/B/R/C/U + egress control
    std::uint64_t acs_redirects = 0;
    std::uint16_t sriov_ctl = 0;              // bit0 VF enable, bit1 migration
    std::uint16_t sriov_status = 0;
    std::uint16_t sriov_initial_vfs = 0, sriov_total_vfs = 0;
    std::uint16_t sriov_vf_offset = 0, sriov_vf_stride = 0;
    std::uint16_t sriov_vf_device_id = 0, sriov_support_page = 0;
    std::uint32_t sriov_vf_bar0 = 0;
    std::uint64_t sriov_vf_enables = 0;

    // ---- Slot power / presence state --------------------------------------
    bool present = true;
    bool power_on = true;
    CxlVppbPowerState power_state = CxlVppbPowerState::ON;
    std::uint32_t power_ramp_ticks = 0;    // 0 => immediate power-on
    std::uint32_t ramp_remaining = 0;
    std::vector<CxlVppbPowerRail> power_rails;   // F3 (empty => legacy ramp)
    std::uint32_t power_reset_ticks = 0;         // PERST# release after last rail
    std::uint32_t power_total_ticks = 0;         // computed for this power-on
    std::uint32_t power_rails_good = 0;          // observability
    std::uint64_t presence_events = 0, power_events = 0;
    std::uint64_t power_faults = 0, power_denied = 0;
    std::uint64_t command_completions = 0;
    std::uint64_t ro_writes = 0;

    bool read(std::uint32_t off, std::uint32_t* val, std::string* err = nullptr) const;
    bool write(std::uint32_t off, std::uint32_t val, std::string* err = nullptr);
    void advance_ticks(std::uint32_t n);

    std::uint8_t pm_state() const { return (std::uint8_t)(pmcsr & 0x3u); }
    bool raise_pme();                        // PME_Status = 1 if PME_En
    bool slot_interrupt_pending() const;     // per-event enable + HPIE/CCIE
    void set_power_fault(bool fault);
    bool set_power_request(bool on, std::string* err = nullptr);
    // F2: extended-capability behavior hooks.
    void inject_aer(bool correctable, std::uint32_t bits);
    void set_aer_header_log(const std::uint32_t dw[4]);
    bool acs_p2p_allowed() const;             // !P2P Request Redirect
    void ats_invalidate();                    // request + completion counters
    bool sriov_set_vf_enable(bool enable);
    // F3: rail/board timing configuration.  Max 8 rails.
    bool add_power_rail(const std::string& name, std::uint32_t delay_ticks,
                        std::uint32_t ramp_ticks, std::uint32_t power_good_ticks,
                        std::string* err = nullptr);
    void clear_power_rails();
    void set_power_reset_ticks(std::uint32_t ticks);
    std::uint32_t power_timeline_ticks() const;   // total for the last request
    std::uint32_t power_rails_good_count() const { return power_rails_good; }
};

struct CxlMldDvsec {
    std::uint16_t cap_id = 0x0009;
    std::uint8_t version = 1;
    std::uint16_t num_lds = 0;
    std::uint16_t ld_ids[16] = {};
    std::uint64_t memory_size = 0;       // total across LDs
    std::uint64_t ld_memory_size = 0;    // per-LD
    std::uint16_t vcs_id = 0;
    std::uint16_t flags = 0;             // bit0 FM-owned, bit1 shared
    bool read(std::uint32_t off, std::uint32_t* val, std::string* err = nullptr) const;
};

struct CxlLogicalDevice {
    std::uint8_t ld_id = 0;
    bool valid = true;
    std::uint64_t size = 16ull << 20;
    std::map<std::uint64_t, std::array<std::uint8_t, 64>> mem;
    std::uint64_t reads = 0, writes = 0;
};

struct CxlMldDevice {
    std::uint16_t dpid = 0;
    bool fmld = false;                 // fabric MLD
    std::vector<CxlLogicalDevice> lds;
    CxlMldDvsec dvsec;
};

struct CxlVppbBind {
    bool bound = false;
    std::uint16_t dpid = 0;
    std::uint8_t ld_id = 0;
    int phys_port = 0;
};

struct CxlVcs {
    std::uint8_t vcs_id = 0;
    std::uint16_t host_pid = 0x0FFF;
    std::vector<CxlVppbBind> vppb;
    std::vector<CxlVppbConfig> cfg;
    std::uint64_t dpc_events = 0;
};

struct CxlMldVcsSystem {
    bool pbr_mode = true;              // MLD/vCS require the PBR profile
    std::map<std::uint16_t, CxlMldDevice> mlds;
    std::map<std::uint8_t, CxlVcs> vcs;

    std::uint64_t binds = 0, accesses = 0, invalid_ld = 0;
    std::uint64_t invalid_vcs = 0, unbound = 0, unsupported = 0, dpc_events = 0;

    void init(bool pbr = true);
    bool add_mld(std::uint16_t dpid, int ld_count, bool fmld,
                 std::uint64_t ld_size = 16ull << 20, std::string* err = nullptr);
    bool add_vcs(std::uint8_t vcs_id, std::uint16_t host_pid, int vppb_count,
                 std::string* err = nullptr);
    bool bind_vppb(std::uint8_t vcs_id, int vppb_idx, std::uint16_t dpid,
                   std::uint8_t ld_id, int phys_port, std::string* err = nullptr);

    bool vcs_write(std::uint8_t vcs_id, int vppb_idx, std::uint64_t dpa,
                   const void* data, std::size_t n, std::string* err = nullptr);
    bool vcs_read(std::uint8_t vcs_id, int vppb_idx, std::uint64_t dpa,
                  void* data, std::size_t n, std::string* err = nullptr);

    bool ld_write(std::uint16_t dpid, std::uint8_t ld_id, std::uint64_t dpa,
                  const void* data, std::size_t n, std::string* err = nullptr);
    bool ld_read(std::uint16_t dpid, std::uint8_t ld_id, std::uint64_t dpa,
                 void* data, std::size_t n, std::string* err = nullptr);

    bool trigger_virtual_dpc(std::uint8_t vcs_id, std::uint8_t ld_id,
                             std::string* err = nullptr);

    // vPPB configuration / hot-plug / MLD DVSEC (P3.5).
    CxlVppbConfig* vppb_cfg(std::uint8_t vcs_id, int vppb_idx);
    const CxlVppbConfig* vppb_cfg(std::uint8_t vcs_id, int vppb_idx) const;
    CxlMldDvsec* mld_dvsec(std::uint16_t dpid);
    bool vppb_read(std::uint8_t vcs_id, int vppb_idx, std::uint32_t off,
                   std::uint32_t* val, std::string* err = nullptr);
    bool vppb_write(std::uint8_t vcs_id, int vppb_idx, std::uint32_t off,
                    std::uint32_t val, std::string* err = nullptr);
    bool set_presence(std::uint8_t vcs_id, int vppb_idx, bool present,
                      std::string* err = nullptr);
    bool set_power(std::uint8_t vcs_id, int vppb_idx, bool on,
                   std::string* err = nullptr);
    void advance_ticks(std::uint32_t n);
    bool unbind_vppb(std::uint8_t vcs_id, int vppb_idx, std::string* err = nullptr);
    bool remove_vcs(std::uint8_t vcs_id, std::string* err = nullptr);
};

} // namespace cxl_tlm
