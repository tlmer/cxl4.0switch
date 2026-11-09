// -----------------------------------------------------------------------------
// pcie_ext_caps.h -- standard PCIe extended capability descriptors.
//
//   A model component's extended capability chain must be selectable per
//   customer DUT (different devices expose different capability sets and
//   field values).  This file provides a small descriptor (`PcieExtCap`) and
//   standard-layout factories; `CxlCfgSpace::add_ext_cap()` lays them out
//   after the built-in CXL capabilities and links the chain.
//
//   Layout sources:
//     - PCIe Base spec 6.x chapter 7 (extended capabilities)
//     - Linux include/uapi/linux/pci_regs.h (register offsets/masks)
//     - Linux include/uapi/linux/pci_regs.h capability IDs
//
//   Each descriptor body starts after the 4-byte extended capability header.
//   `rw_mask[d]` is the RW mask for body dword d; a dword without a mask is
//   read-only.  Firmware-visible fields that the model owns (capability
//   registers) are initialized here; data-plane consumers use the same
//   registers through CxlCfgSpace.
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace cxl_tlm {

struct PcieExtCap {
    std::uint16_t id = 0;
    std::uint8_t  version = 1;
    std::uint16_t off = 0;            // assigned by CxlCfgSpace::add_ext_cap
    std::uint16_t size = 4;           // total size including the header
    std::vector<std::uint8_t> body;   // size-4 bytes after the header
    std::vector<std::uint32_t> rw_mask;  // per body dword (absent => RO)
    // Optional register-specific write hook: (offset relative to the cap,
    // access size, value).  Return true when it consumed the write.
    std::function<bool(std::uint16_t, unsigned, std::uint32_t)> hook;

    std::uint32_t rd32(std::uint16_t rel) const {
        std::uint32_t v = 0;
        for (int i = 0; i < 4 && (std::size_t)(rel + i) < body.size(); ++i)
            v |= (std::uint32_t)body[(std::size_t)(rel + i)] << (8 * i);
        return v;
    }
};

namespace pcie_ext {

// ---- Capability IDs (Linux pci_regs.h) ------------------------------------
constexpr std::uint16_t kAer         = 0x0001;
constexpr std::uint16_t kVc          = 0x0002;
constexpr std::uint16_t kDsn         = 0x0003;
constexpr std::uint16_t kPower       = 0x0004;
constexpr std::uint16_t kRcld        = 0x0005;
constexpr std::uint16_t kRcilc       = 0x0006;
constexpr std::uint16_t kRcec        = 0x0007;
constexpr std::uint16_t kMfvc        = 0x0008;
constexpr std::uint16_t kVc9         = 0x0009;
constexpr std::uint16_t kRcrb        = 0x000A;
constexpr std::uint16_t kVendor      = 0x000B;
constexpr std::uint16_t kCac         = 0x000C;
constexpr std::uint16_t kAcs         = 0x000D;
constexpr std::uint16_t kAri         = 0x000E;
constexpr std::uint16_t kAts         = 0x000F;
constexpr std::uint16_t kSriov       = 0x0010;
constexpr std::uint16_t kMriov       = 0x0011;
constexpr std::uint16_t kMcast       = 0x0012;
constexpr std::uint16_t kPri         = 0x0013;
constexpr std::uint16_t kRebar       = 0x0015;
constexpr std::uint16_t kDpa         = 0x0016;
constexpr std::uint16_t kTph         = 0x0017;
constexpr std::uint16_t kLtr         = 0x0018;
constexpr std::uint16_t kSecPcie     = 0x0019;
constexpr std::uint16_t kPmux        = 0x001A;
constexpr std::uint16_t kPasid       = 0x001B;
constexpr std::uint16_t kDpc         = 0x001D;
constexpr std::uint16_t kL1ss        = 0x001E;
constexpr std::uint16_t kPtm         = 0x001F;
constexpr std::uint16_t kDvsec       = 0x0023;
constexpr std::uint16_t kVfRebar     = 0x0024;
constexpr std::uint16_t kDlf         = 0x0025;
constexpr std::uint16_t kPl16        = 0x0026;
constexpr std::uint16_t kNpem        = 0x0029;
constexpr std::uint16_t kPl32        = 0x002A;
constexpr std::uint16_t kDoe         = 0x002E;
constexpr std::uint16_t kPl64        = 0x0031;

// ---- Helpers --------------------------------------------------------------
void put16(PcieExtCap& c, std::uint16_t rel, std::uint16_t v);
void put32(PcieExtCap& c, std::uint16_t rel, std::uint32_t v);
void set_rw(PcieExtCap& c, std::uint16_t rel, std::uint32_t mask);

// ---- Standard-layout factories -------------------------------------------
// Device Serial Number (12 bytes total; 64-bit RO serial).
PcieExtCap dsn(std::uint64_t serial);

// Power Budgeting (0x10 total): Data Select RW, Data/Capability RO.
PcieExtCap power_budgeting(std::uint8_t base_power, std::uint8_t scale,
                           bool included_in_budget, std::uint8_t pm_substate,
                           std::uint8_t pm_state, std::uint8_t type,
                           std::uint8_t rail);

// Page Request Interface (0x10 total): control RW, status RW1C, max/alloc RW.
// `pasid_required` sets the RO "PRG Response PASID Required" status bit.
// `max_requests` is stored as log2(max_requests) in both request registers.
PcieExtCap pri(bool pasid_required, std::uint8_t max_requests_log2);

// TPH Requester (0xC + 2*st_entries): capability RO, control RW, ST table RW.
PcieExtCap tph(std::uint16_t st_entries);

// Latency Tolerance Reporting (8 bytes): both latency registers RW.
PcieExtCap ltr(std::uint16_t max_snoop, std::uint16_t max_nosnoop);

// Dynamic Power Allocation (0x10): capability RO, control RW, status RO.
PcieExtCap dpa(std::uint8_t substates);

// Secondary PCI Express (0xC + 2*lanes): Link Control 3 + lane EQ RW.
PcieExtCap secondary_pcie(unsigned lanes);

// Protocol Multiplexing (0x10): capability RO, control RW, status RO.
PcieExtCap pmux(std::uint32_t capability);

// Data Link Feature (8 bytes): exchange-enable bit RW.
PcieExtCap dlf(bool exchange_enable);

// Physical Layer 16.0 GT/s (0x20 + 2*lanes): lane EQ control RW.
PcieExtCap pl16(unsigned lanes);

// Physical Layer 32.0 GT/s (0x20 + 2*lanes): lane EQ control RW.
PcieExtCap pl32(unsigned lanes);

// Physical Layer 64.0 GT/s (0x20 + 2*lanes): lane EQ control RW.
PcieExtCap pl64(unsigned lanes);

// Native PCIe Enclosure Management (0x10): capability RO, control RW,
// status with Command Completed (RW1C via hook).
PcieExtCap npem();

// Alternate Routing ID (0x10): capability RO, control RW.
PcieExtCap ari();

// VF Resizable BAR (0xC + 4*vf_bars): capability RO, control + sizes RW.
PcieExtCap vf_rebar(unsigned vf_bars);

// Generic Designated Vendor-Specific capability (ID 0023).
PcieExtCap dvsec(std::uint16_t vendor, std::uint8_t revision,
                 std::uint16_t dvsec_id, const std::vector<std::uint8_t>& data);

// Vendor-Specific extended capability (ID 000B; VSEC ID/Revision/Length).
PcieExtCap vsec(std::uint16_t vsec_id, std::uint8_t revision,
                const std::vector<std::uint8_t>& data);

// Data Object Exchange (0x18): capability RO, control RW, mailboxes.
PcieExtCap doe(bool interrupt_support);

// Precision Time Measurement (0xC): capability RO, control RW.
PcieExtCap ptm();

// L1 PM Substates (0x14): capability RO, Control 1/2 RW, status RO.
PcieExtCap l1ss();

// Configuration Access Correlation (0x0C, obsolete): header-only descriptor.
PcieExtCap cac();

// Virtual Channel: standard port VC capability/control/status, one VC
// Resource block per VC, and a per-VC Port Arbitration Table.  Layout per
// PCIe Base 6.x 7.9.2 and Linux pci_regs.h offsets:
//   +04 Port VC Capability 1   +08 Port VC Capability 2
//   +0C Port VC Control        +0E Port VC Status
//   +10 VC Resource 0 Cap      +14 VC Resource 0 Control
//   +18 reserved               +1A VC Resource 0 Status
//   +1C.. tables                (next VC resource block: +0C stride)
// `vc_count` = 1..8 (Port VC Count - 1 in EVCC), `pat_phases` = 32/64/128.
PcieExtCap vc(unsigned vc_count = 1, unsigned pat_phases = 32);

// Legacy VC capability alias (ID 0x09) with the same register block.
PcieExtCap vc9(unsigned vc_count = 1, unsigned pat_phases = 32);

// Multi-Function Virtual Channel: MFVC port capability/control/status plus
// one VC Resource block per function.  MFVC has no TC/VC map; the model
// exposes the register face and per-function arbitration selects.
PcieExtCap mfvc(unsigned functions = 4);

// Root Complex Event Collector (0xC): RCiEP bitmap + bus numbers.
PcieExtCap rcec(std::uint32_t rc_iep_bitmap, std::uint8_t bus_next,
                std::uint8_t bus_last);

// Root Complex Register Block (8 bytes): base address RW.
PcieExtCap rcrb(std::uint64_t base);

// Root Complex Link Declaration: Element Self Description plus one
// Link Description / Link Address / Element Link Control / Link Capability
// group per link (0x1C + 0x14*(links-1) total).
PcieExtCap rcld(unsigned links = 1, std::uint8_t element_type = 1,
                std::uint8_t element_id = 0);

// Root Complex Internal Link Control (0x10): element ID, RCL control
// (link disable) and status.
PcieExtCap rcilc(std::uint8_t element_id = 0);

// Multi-Root I/O Virtualization: MR-IOV capability/control/status plus the
// VH (virtual hierarchy) capability/control/status/group-ID registers.  The
// fabric multi-root data path is out of the single-root model profile; the
// registers are present so a multi-root DUT's capability layout is mirrored.
PcieExtCap mriov(bool supported = false, unsigned vh_groups = 0);

} // namespace pcie_ext
} // namespace cxl_tlm
