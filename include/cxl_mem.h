// -----------------------------------------------------------------------------
// cxl_mem.h -- CXL.mem data plane skeleton (M3): host agent <-> switch <-> TLM
// Type3 stub.
//
//   Each operation is encoded into a real M2S/S2M message (cxl_msg), routed by
//   the switch HDM, applied to the port's backend memory, and answered with the
//   reflected Tag.  This exercises the channel-level message formats, HDM
//   routing, BE merging, poison and NXM handling.
//
//   v1: 64B-aligned accesses up to one cacheline; 68B or 256B(HBR) messages.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_hdm_prog.h"
#include "cxl_msg.h"
#include "cxl_port_adapter.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlMemSystem {
    CxlRoot root;
    CxlSwitch sw;
    std::vector<CxlPortAdapter> adapters;
    CxlFlitMode mode = CxlFlitMode::B68;
    std::vector<CxlHdmRegion> regions;

    std::map<std::uint64_t, bool> poison_lines;   // key = DPA line (64B aligned)

    // Statistics (never silent).
    std::uint64_t reads = 0, writes = 0, partial_writes = 0;
    std::uint64_t poison_hits = 0, nxm = 0, tag_errors = 0, be_errors = 0;

    // Test hook for the tag-mismatch red mode.
    bool inject_bad_tag = false;

    // ---------------------------------------------------- multi-outstanding
    struct Pending {
        std::uint16_t tag = 0;
        std::uint64_t seq = 0;
        std::uint64_t hpa = 0;
        std::size_t n = 0;
        std::uint64_t be = ~0ull;
        bool write = false, poison = false;
        std::vector<std::uint8_t> data;      // write payload / read buffer
    };
    std::vector<Pending> outstanding;
    std::uint64_t submitted = 0, completed = 0;
    std::uint64_t duplicate_tags = 0, unknown_tags = 0, ordering_errors = 0;
    bool allow_write_reorder = false;        // red fault hook

    bool init(int n, std::uint64_t dpa_bytes = 16ull << 20, std::string* err = nullptr);

    bool write(std::uint64_t hpa, const void* data, std::size_t n,
               std::uint64_t be = ~0ull, bool poison = false, std::uint16_t tag = 0,
               std::string* err = nullptr);
    bool read(std::uint64_t hpa, void* data, std::size_t n, std::uint16_t tag = 0,
              bool* poison = nullptr, std::string* err = nullptr);

    // Asynchronous Tag-tracked path.
    bool submit_read(std::uint64_t hpa, std::size_t n, std::uint16_t tag,
                     std::string* err = nullptr);
    bool submit_write(std::uint64_t hpa, const void* data, std::size_t n,
                      std::uint64_t be = ~0ull, bool poison = false,
                      std::uint16_t tag = 0, std::string* err = nullptr);
    bool complete(std::uint16_t tag, void* data = nullptr, bool* poison = nullptr,
                  std::string* err = nullptr);
    bool pending(std::uint16_t tag) const;
};

} // namespace cxl_tlm
