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
#include "cxl_hdm.h"

#include <cstdint>
#include <cstring>
#include <string>
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

    // MSI / IRQ table.
    void enrol_irq(std::uint16_t reqid, std::uint8_t vector, std::uint64_t addr,
                   std::uint32_t data);
    int raise_msi(std::uint64_t addr, std::uint32_t data);   // returns vector or -1
};

} // namespace cxl_tlm
