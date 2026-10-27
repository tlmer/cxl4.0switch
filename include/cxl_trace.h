// -----------------------------------------------------------------------------
// cxl_trace.h -- MODEL_TRACE support (checklist T3).
//
//   MODEL_TRACE non-empty enables the trace.  Six node classes are emitted so
//   a validation run can prove all of them fired:
//     STAGE   -- sequence / testcase stage banners
//     IRQ     -- interrupt or event delivery
//     EXC     -- unexpected protocol / model state
//     ERR     -- explicit error (never silent)
//     RESULT  -- per-run result
//     SUM     -- run summary
//   Tracing is observational only; it does not change model behavior.
// -----------------------------------------------------------------------------
#pragma once
#include <cstdint>

namespace cxl_tlm {

enum CxlTraceClass : int {
    CXL_TRACE_STAGE = 0,
    CXL_TRACE_IRQ = 1,
    CXL_TRACE_EXC = 2,
    CXL_TRACE_ERR = 3,
    CXL_TRACE_RESULT = 4,
    CXL_TRACE_SUM = 5,
    CXL_TRACE_CLASS_COUNT = 6
};

const char* cxl_trace_class_name(CxlTraceClass c);
bool cxl_trace_enabled();
// on: 1=enable, 0=disable, -1=follow MODEL_TRACE.
void cxl_trace_set(int on);
void cxl_trace_reset_counters();
void cxl_trace_counts(std::uint64_t out[CXL_TRACE_CLASS_COUNT]);
void cxl_trace_event(CxlTraceClass c, const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
const char* cxl_trace_sink();     // last formatted trace line (for tests)

} // namespace cxl_tlm
