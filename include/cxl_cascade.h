// -----------------------------------------------------------------------------
// cxl_cascade.h -- two-level HBR cascade (M5.7/M5.12).
//
//   outer switch DSP -> inner switch USP -> inner DSPs.  Each level has its own
//   HDM decoder chain; the model resolves HPA through outer USP -> inner USP ->
//   inner DSP -> device DPA.  More than two levels is UNSUPPORTED (P2).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_port_adapter.h"
#include "cxl_switch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlCascade {
    static constexpr int kMaxDepth = 2;

    CxlSwitch outer;
    CxlSwitch inner;
    int cascade_port = 0;                 // outer DSP that connects the inner USP
    std::vector<CxlPortAdapter> adapters; // inner DSP backends
    std::uint64_t reads = 0, writes = 0, misses = 0;

    bool init(int inner_ports = 2, std::string* err = nullptr);
    bool program_hdm(std::uint64_t base, std::uint64_t size, int inner_port,
                     std::string* err = nullptr);
    bool mem_route(std::uint64_t hpa, int* inner_port, std::uint64_t* dpa,
                   std::string* err = nullptr);
    bool write(std::uint64_t hpa, const void* data, std::size_t n,
               std::string* err = nullptr);
    bool read(std::uint64_t hpa, void* data, std::size_t n, std::string* err = nullptr);

    std::string topology() const;
};

} // namespace cxl_tlm
