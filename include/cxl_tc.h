// -----------------------------------------------------------------------------
// cxl_tc.h -- CXL model testcase registry (checklist T2/T4/T5/T6).
//
//   The registry is linkable into the delivered library.  It drives the model
//   through the same public classes a customer would use; every case returns
//   false on the first mismatch and logs the reason.  CXL_TC_RUN with
//   id = CXL_TC_ID_ALL runs every case (T5); iterations replays one case (T6).
//
//   The ioctl-style dispatcher below is the C-side integration point; an OS
//   character device would forward its unlocked_ioctl to it.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_tc_ref.h"

#include <cstdint>
#include <string>

namespace cxl_tlm {

struct CxlTcCaseInfo {
    std::uint32_t id = 0;
    std::string name;
    std::string description;
};

std::uint32_t cxl_tc_get_count();

// id is 0-based.  Returns false and sets err for an out-of-range id.
bool cxl_tc_get_info(std::uint32_t id, CxlTcCaseInfo* out, std::string* err = nullptr);

// Run one case (id < count) or all (id == CXL_TC_ID_ALL).  `iterations` is
// clamped to [1, 10000].  Each case is recreated from scratch per iteration.
bool cxl_tc_execute(std::uint32_t id, std::uint32_t iterations,
                    cxl_tc_run* result, std::string* log = nullptr);

// Coverage report: run/pass counts plus the untested list and reasons (T4).
bool cxl_tc_get_coverage(cxl_tc_coverage* cov);

// ioctl-style dispatch.  Supports CXL_TC_GET_COUNT/GET_INFO/RUN/RUN_ALL/
// COVERAGE.  Unknown requests fail (never silently ignored).
int cxl_tc_ioctl(unsigned long request, void* arg, std::string* err = nullptr);

#if defined(CXL_TC_TEST_HOOKS)
// Test-only hook used to prove the runner reports failures instead of
// silently passing.  Never compiled into the delivered library.
void cxl_tc_register_injected_failure(const char* name);
void cxl_tc_clear_injected_failures();
#endif

} // namespace cxl_tlm
