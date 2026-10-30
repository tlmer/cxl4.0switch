// -----------------------------------------------------------------------------
// cxl_hdm_prog.h -- multi-level HDM programming (M2.7).
//
//   Order (CXL Type-3 SW Guide / design Sec. 5.3):
//     endpoint/device -> DSP -> USP -> root, each committed and verified.
//   v1 uses one 1-way decoder per port (no interleave): each region maps a
//   disjoint HPA window to one DSP.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_root.h"
#include "cxl_switch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlHdmRegion {
    int port = 0;
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    // Cascaded-switch path below `port`: path[0] is the child DSP port,
    // path[1] the grandchild DSP port, and so on.  Empty = endpoint directly
    // below the top DSP.
    std::vector<int> path;
};

// Programs DSP/USP/root decoders for every region.  Fails (with a reason) on
// overlap, alignment, out-of-range port, link-down port, or commit error.
bool cxl_program_hdm(CxlRoot& root, CxlSwitch& sw,
                     const std::vector<CxlHdmRegion>& regions,
                     std::string* err = nullptr);

// Verifies root -> USP -> DSP routing for one HPA.
bool cxl_verify_route(const CxlRoot& root, const CxlSwitch& sw, std::uint64_t hpa,
                      int expect_port, std::string* err = nullptr);

} // namespace cxl_tlm
