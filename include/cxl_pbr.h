// -----------------------------------------------------------------------------
// cxl_pbr.h -- PBR routing skeleton (M5.1/M5.3/M5.6) and Direct P2P CXL.mem.
//
//   PTH (CXL 4.0 Section 3.1.8) carries SPID/DPID on CXL.io TLPs.  For
//   CXL.cachemem PBR messages the same SPID/DPID values live in the message
//   header (cxl_msg PBR layouts).  This model routes by DPID using the
//   FAST/LDST tables; the Edge DSP link side belongs to the customer swdn.
//
//   Direct P2P: a device on one DSP writes/reads another DSP's memory without
//   traversing host memory; the switch routes by DPID.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_msg.h"
#include "cxl_pth.h"
#include "cxl_port_adapter.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlPbrFastEntry {
    std::uint16_t spid = 0, dpid = 0;
    int port = 0;
};

struct CxlPbrRouter {
    std::uint16_t self_pid = 0;
    std::vector<CxlPbrFastEntry> fast;
    std::vector<std::pair<std::uint16_t, int>> ldst;   // DPID -> egress port
    mutable std::uint64_t fast_hits = 0, ldst_hits = 0, self_hits = 0;
    mutable std::uint64_t misses = 0, loops = 0, unsupported = 0;

    void add_fast(std::uint16_t spid, std::uint16_t dpid, int port);
    void add_ldst(std::uint16_t dpid, int port);
    // egress = -1 => delivered locally (DPID == self); false => miss/unsupported.
    bool route(std::uint16_t spid, std::uint16_t dpid, int ingress, int* egress,
               std::string* err = nullptr) const;
};

struct CxlPbrSystem {
    CxlPbrRouter router;
    std::vector<std::unique_ptr<CxlTlmStubBackend>> dev;
    std::vector<std::uint16_t> pid;
    std::uint64_t p2p_writes = 0, p2p_reads = 0, p2p_misses = 0;

    bool init(int n, std::uint16_t base_pid = 0x0100,
              std::uint64_t dpa_bytes = 16ull << 20);
    std::uint16_t dpid_of(int port) const { return pid[(size_t)port]; }

    // Device-to-device CXL.mem access (Direct P2P).  src is the ingress DSP.
    bool device_write(int src_port, std::uint16_t dst_dpid, std::uint64_t dpa,
                      const void* data, std::size_t n, std::string* err = nullptr);
    bool device_read(int src_port, std::uint16_t dst_dpid, std::uint64_t dpa,
                     void* data, std::size_t n, std::string* err = nullptr);

    CxlTlmStubBackend* backend(int port) {
        return (port >= 0 && port < (int)dev.size()) ? dev[(size_t)port].get() : nullptr;
    }
};

// PBR message round-trip helper (SPID/DPID in the PBR header).
bool cxl_pbr_msg_roundtrip(std::uint16_t spid, std::uint16_t dpid,
                           std::uint64_t hpa, std::uint16_t tag, std::string* err);

} // namespace cxl_tlm
