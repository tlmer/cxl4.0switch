// -----------------------------------------------------------------------------
// cxl_root.h -- model CXL root (host side): root HDM decoder, host memory,
// IRQ/MSI collection.  M2.1 skeleton.
//
//   The root has no Type0 configuration of its own in this model (the RC is
//   the initiator; the USP is enumerated at bus 0 / dev 0 per design Sec. 2.2).
//   CXL.io configuration forwarding lives in CxlSwitch (M2.6).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_axi_master.h"
#include "cxl_cper.h"
#include "cxl_hdm.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace cxl_tlm {

struct CxlRoot {
    CxlHdm hdm;                                  // HOST_BRIDGE decoder
    std::vector<std::uint8_t> host_mem;          // HPA backing store (internal)
    std::uint64_t host_mem_base = 0;
    // Optional external DDR: when non-null, host memory requests are issued as
    // 128-bit AXI4 bursts instead of touching host_mem.  The on-board SRAM is
    // outside the model and is connected on this master port.
    CxlAxiMaster* axi = nullptr;
    std::uint64_t axi_reads = 0, axi_writes = 0, axi_errors = 0;
    void attach_axi(CxlAxiMaster* m) { axi = m; }

    struct IrqEntry {
        std::uint16_t reqid = 0;
        std::uint8_t  vector = 0;
        std::uint64_t addr = 0;
        std::uint32_t data = 0;
        std::uint64_t hits = 0;
    };
    std::vector<IrqEntry> irqs;
    std::uint64_t irq_asserts = 0, irq_misses = 0;

    // PCIe Message receive (OS-like root behavior).  Messages are separate
    // from MSI: they carry no completion and are consumed by the root.
    struct MessageState {
        std::uint64_t aer_cor = 0, aer_nonfatal = 0, aer_fatal = 0;
        std::uint32_t aer_cor_status = 0, aer_uc_status = 0;
        std::uint16_t aer_source = 0;
        std::uint64_t pme = 0, pme_turn_off = 0, pme_to_ack = 0;
        std::uint64_t ltr = 0, obff = 0, vdm = 0;
        std::uint64_t ptm_req = 0, ptm_res = 0;
        std::uint64_t intx_assert = 0, intx_deassert = 0, other = 0;
        std::uint16_t last_reqid = 0;
        std::uint8_t  last_tag = 0;
        std::uint32_t last_dw0 = 0, last_dw1 = 0;
        std::uint8_t  intx_state = 0;      // INTA..INTD asserted
        bool          pme_pending = false;
        // RAS: the last AER message is materialized as a CXL Protocol Error
        // CPER record so an OS-like consumer can forward it directly.
        std::vector<std::uint8_t> last_cper;
        std::uint64_t cper_records = 0, cper_failures = 0;
        // PCIe Error CPER section (AER status/mask/header-log image).
        std::vector<std::uint8_t> last_pcie_cper;
        std::uint64_t pcie_cper_records = 0, pcie_cper_failures = 0;
    };
    MessageState msg;

    // Consume a PCIe Message TLP (code = message code, 8 bits).
    void receive_message(std::uint16_t code, std::uint16_t reqid,
                         std::uint8_t tag, std::uint32_t dw0,
                         std::uint32_t dw1);

    // Drive an AER error as if the given agent reported it (root-side error
    // injection for RAS testing).  Updates the OS-like counters and builds a
    // CXL Protocol Error CPER record; zero status bits is rejected.
    bool inject_aer(bool correctable, std::uint32_t status_bits, bool fatal,
                    std::uint64_t timestamp, std::string* err = nullptr);

    // Shared AER accounting + CPER build used by receive_message()/inject_aer().
    void note_aer(std::uint16_t code, std::uint32_t status_bits,
                  std::uint16_t source_reqid, std::uint64_t timestamp,
                  std::uint8_t agent_type = CPER_AGENT_DEVICE);

    // Build an OS-like PCIe Error CPER section for an AER event (separate
    // from the CXL Protocol Error section above).  Zero status bits are
    // rejected; the record carries the AER status/mask/header-log image.
    bool inject_pcie_aer(bool correctable, std::uint32_t status_bits,
                         std::uint16_t source_reqid, bool fatal,
                         std::uint64_t timestamp, std::string* err = nullptr);

    // PASID translation table (model policy; HPA = base + IOVA).  PASID 0 is
    // reserved and rejected; unknown/overflowing translations are explicit.
    bool set_pasid_base(std::uint32_t pasid, std::uint64_t base);
    bool clear_pasid(std::uint32_t pasid);
    bool pasid_translate(std::uint32_t pasid, std::uint64_t iova,
                         std::uint64_t* hpa, std::string* err = nullptr);
    // PASID-tagged host memory access: translate IOVA -> HPA, then use the
    // normal host-memory/AXI path.  Unknown PASID / overflow are explicit.
    bool mem_read_pasid(std::uint32_t pasid, std::uint64_t iova, void* dst,
                        std::size_t n, std::string* err = nullptr);
    bool mem_write_pasid(std::uint32_t pasid, std::uint64_t iova,
                         const void* src, std::size_t n,
                         std::string* err = nullptr);
    std::unordered_map<std::uint32_t, std::uint64_t> pasid_bases;
    std::uint64_t pasid_translate_failures = 0;

    // ATS translation policy (host side).  The root keeps a per-requester
    // IOVA->HPA mapping table, resolves translated requests, and drops
    // entries on Invalidate Requests.  Granularity is the PCIe minimum
    // (4 KB): bases and sizes must be page aligned.  Overlapping mappings
    // for the same requester are rejected instead of silently shadowing.
    struct AtsMapping {
        std::uint16_t reqid = 0;
        std::uint64_t iova_base = 0;
        std::uint64_t hpa_base = 0;
        std::uint64_t size = 0;
    };
    std::vector<AtsMapping> ats_maps;
    std::uint64_t ats_hits = 0, ats_misses = 0, ats_inv_entries = 0;
    std::uint64_t ats_map_errors = 0;

    bool ats_map(std::uint16_t reqid, std::uint64_t iova_base,
                 std::uint64_t hpa_base, std::uint64_t size,
                 std::string* err = nullptr);
    bool ats_translate(std::uint16_t reqid, std::uint64_t iova,
                       std::uint64_t* hpa, std::string* err = nullptr);
    // Range variant: a single mapping must cover the whole [iova, iova+n)
    // range (a request may not split across two mappings).
    bool ats_translate_range(std::uint16_t reqid, std::uint64_t iova,
                             std::uint64_t n, std::uint64_t* hpa,
                             std::string* err = nullptr);
    // Drop mappings for one requester: entire space (size == 0) or every
    // mapping that overlaps [iova_base, iova_base+size).  Returns the number
    // of entries removed; overlap removal is conservative (whole entry).
    unsigned ats_invalidate(std::uint16_t reqid, std::uint64_t iova_base = 0,
                            std::uint64_t size = 0);

    void init(int target_ports, std::uint64_t mem_bytes = 64ull << 20,
              std::uint64_t mem_base = 0);
    // Standard root: N decoders but the decoder target list is the root port
    // (one target for the q35 model), not the DSP port number.
    void init(int decoders, int targets, std::uint64_t mem_bytes,
              std::uint64_t mem_base);
    void reset();

    // Root HDM programming helpers (registers are relative to the HDM block).
    bool program_decoder(int n, std::uint64_t base, std::uint64_t size,
                         std::uint32_t target_port, std::string* err = nullptr);
    bool map_hpa(std::uint64_t hpa, int* port, std::uint64_t* offset,
                 std::string* err = nullptr) const;

    // Host memory access (HPA must be inside [base, base+size)).  With an AXI
    // master attached the request crosses the 128-bit AXI4 port; otherwise the
    // internal host_mem vector is used (legacy model profile).
    bool mem_read(std::uint64_t hpa, void* dst, std::size_t n, std::string* err = nullptr);
    bool mem_write(std::uint64_t hpa, const void* src, std::size_t n, std::string* err = nullptr);

    // ATS-tagged host memory access: translate IOVA -> HPA with the root's
    // per-requester ATS table, then use the normal host-memory/AXI path.
    // Unknown/multi-mapping accesses are rejected explicitly.
    bool mem_read_ats(std::uint16_t reqid, std::uint64_t iova, void* dst,
                      std::size_t n, std::string* err = nullptr);
    bool mem_write_ats(std::uint16_t reqid, std::uint64_t iova,
                       const void* src, std::size_t n,
                       std::string* err = nullptr);

    // MSI / IRQ table.
    void enrol_irq(std::uint16_t reqid, std::uint8_t vector, std::uint64_t addr,
                   std::uint32_t data);
    int raise_msi(std::uint64_t addr, std::uint32_t data);   // returns vector or -1
};

} // namespace cxl_tlm
