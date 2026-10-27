// -----------------------------------------------------------------------------
// cxl_switch.h -- CXL switch skeleton: USP + N DSP ports + CXL.io config
// routing + HBR (HPA) routing.  M2.4.
//
//   Bus model (design Sec. 2.2): USP at bus 0/dev 0; internal bus = USP.sec;
//   DSP i at (internal bus, dev i); the device below DSP i lives at
//   DSP_i.sec (customer EP DUT / TLM stub).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_swdn_port.h"
#include "cxl_usp.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlSwitch {
    CxlUsp usp;
    std::vector<CxlSwdnPort> ports;
    std::uint8_t internal_bus = 1;

    // Statistics (never silent).
    std::uint64_t cfg_hits = 0, cfg_ur = 0;
    mutable std::uint64_t mem_hits = 0, mem_miss = 0, loop_drop = 0;

    void init(int n);
    void reset_stats();

    // Model/demo topology defaults (M2.6 will drive the real enumerator).
    void program_topology_defaults();

    bool cfg_access(std::uint8_t bus, std::uint8_t dev, std::uint8_t fn,
                    std::uint16_t off, unsigned size, bool write,
                    std::uint32_t* val, std::string* err = nullptr);

    // Returns the destination port and DPA.  src_port = -1 for upstream (root).
    bool mem_route(std::uint64_t hpa, int src_port, int* dst_port,
                   std::uint64_t* dpa, std::string* err = nullptr) const;
};

} // namespace cxl_tlm
