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
#include "cxl_root_complex.h"
#include "cxl_switch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

enum class CxlEnumRole : std::uint8_t {
    UNKNOWN = 0,
    HOST_BRIDGE,
    ROOT_PORT,
    USP,
    DSP,
    ENDPOINT
};

struct CxlCapRef {
    std::uint16_t id = 0;
    std::uint16_t off = 0;
    bool ext = false;
};

struct CxlEnumDev {
    std::string path;
    CxlEnumRole role = CxlEnumRole::UNKNOWN;
    int parent = -1;                           // index in CxlDeviceTree::devs
    std::uint8_t bus = 0, dev = 0, fn = 0;
    std::uint8_t pcie_cap_type = 0;            // 0/4/5/6
    std::uint8_t sec = 0, sub = 0;              // Type1 downstream range
    std::uint16_t vid = 0, did = 0;
    std::uint8_t header_type = 0;
    std::uint32_t class_code = 0;
    std::uint64_t bar0_base = 0;
    // Linux-like BAR/ROM enumeration (all implemented memory BARs).
    static constexpr int kMaxBars = 6;
    std::uint64_t bar_base[kMaxBars] = {};
    std::uint32_t bar_size[kMaxBars] = {};
    bool bar_impl[kMaxBars] = {};
    // Memory BAR prefetchable bit (PCI BAR bit 3), used to choose between
    // the bridge's memory and prefetchable memory windows.
    bool bar_pref[kMaxBars] = {};
    // True when the endpoint's config space does not implement BAR sizing
    // (readback all-ones) and the model had to use a fixed fallback aperture.
    bool bar_size_estimated[kMaxBars] = {};
    std::uint64_t rom_base = 0;
    std::uint32_t rom_size = 0;
    bool rom_impl = false;
    bool rom_size_estimated = false;
    bool msi_present = false;
    bool msix_present = false;
    bool ltr_present = false;
    // First-read snapshots taken during the capability walk (before any host
    // policy writes).  Some DUT config paths corrupt later reads after a
    // write; use these cached values for MSI/MSI-X programming.
    bool msi_cached = false, msix_cached = false;
    std::uint32_t msi_hdr = 0, msi_addr = 0, msi_data = 0;
    std::uint32_t msix_hdr = 0, msix_tbl = 0, msix_pba = 0;
    std::uint16_t pcie_cap_off = 0;
    std::uint16_t pm_off = 0;                  // standard PM capability offset
    std::uint16_t msi_off = 0;                 // standard MSI capability offset
    std::uint16_t msix_off = 0;                // standard MSI-X capability offset
    std::uint16_t aer_off = 0;                 // extended AER capability offset
    std::uint16_t ltr_off = 0;                 // extended LTR capability offset
    std::uint16_t vpd_off = 0;                 // standard VPD capability offset
    std::uint16_t acs_off = 0;                 // extended ACS capability offset
    std::uint16_t ari_off = 0;                 // extended ARI capability offset
    std::uint16_t ats_off = 0;                 // extended ATS capability offset
    std::uint16_t pasid_off = 0;               // extended PASID capability offset
    std::uint16_t rebar_off = 0;               // extended ReBAR capability offset
    std::uint16_t sriov_off = 0;               // extended SR-IOV capability offset
    std::uint16_t dpc_off = 0;                 // extended DPC capability offset
    std::uint16_t l1ss_off = 0;                // extended L1 PM Substates offset
    std::uint16_t doe_off = 0;                 // extended DOE capability offset
    std::uint16_t ide_off = 0;                 // extended IDE capability offset
    std::uint16_t ptm_off = 0;                 // extended PTM capability offset
    std::uint16_t cxl_dvsec_off = 0;           // CXL Device DVSEC offset
    std::uint16_t regloc_off = 0;              // Register Locator DVSEC offset
    std::uint16_t command = 0;                 // PCI Command (MSE bit 1)
    bool mse = false;
    int std_cap_count = 0;                     // standard PCIe cap chain
    int cap_recovery = 0;                      // caps found by known-offset recovery
    std::vector<std::uint16_t> std_caps;       // cap IDs in chain order
    int ext_cap_count = 0;                     // PCIe extended cap chain
    std::vector<std::uint16_t> ext_caps;       // ext cap IDs in chain order
    std::vector<CxlCapRef> caps;               // every cap (id/offset/ext)
    bool has_cxl_dvsec = false;
    std::uint16_t dvsec_id = 0;
    std::vector<std::uint16_t> dvsec_ids;      // DVSEC IDs in chain order
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

// Legacy switch-only enumerator (fixture for low-level tests; starts at the
// switch USP).
CxlDeviceTree cxl_enumerate(CxlSwitch& sw, const CxlEnumConfig& cfg = CxlEnumConfig());

// Standard Linux-like enumerator: host bridge at 00:00.0, root port at
// 00:01.0, USP on the root-port secondary bus, DSPs and endpoints below.
CxlDeviceTree cxl_enumerate_host(CxlRootComplex& rc, CxlSwitch& sw,
                                 const CxlEnumConfig& cfg = CxlEnumConfig());

bool cxl_app_dev_num_ok(const CxlEnumConfig& cfg, std::uint8_t actual,
                        std::string* err = nullptr);

} // namespace cxl_tlm
