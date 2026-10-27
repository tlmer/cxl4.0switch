// -----------------------------------------------------------------------------
// cxl_smoke.cpp -- independent-compile smoke for the delivered CXL library.
//
//   Built with only -I include and lib/libcxl_switch_model.a.  It must not see
//   model/ sources.  Verifies Linux-proxy initialization, one memory task, the
//   testcase channel, and the performance window.
// -----------------------------------------------------------------------------
#include "cxl_linux.h"
#include "cxl_perf.h"
#include "cxl_tc.h"

#include <cstdio>
#include <cstring>


int main() {
    cxl_tlm::CxlLinuxProxy proxy;
    std::string err;
    if (!proxy.init(2, &err)) {
        std::printf("SMOKE_CXL: FAIL init: %s\n", err.c_str());
        return 1;
    }
    if (!proxy.acpi_view() || !proxy.discover() || !proxy.program_hdm() ||
        !proxy.create_regions() || !proxy.mem_task(0)) {
        std::printf("SMOKE_CXL: FAIL sequence\n%s\n", proxy.log.c_str());
        return 1;
    }
    const cxl_tlm::CxlPerfSnapshot perf = proxy.perf_mem.snapshot();
    if (perf.ops < 1 || perf.bytes < 128) {
        std::printf("SMOKE_CXL: FAIL perf counters\n");
        return 1;
    }
    if (cxl_tlm::cxl_tc_get_count() < 10) {
        std::printf("SMOKE_CXL: FAIL testcase registry\n");
        return 1;
    }
    ::cxl_tc_run run;
    std::memset(&run, 0, sizeof(run));
    if (!cxl_tlm::cxl_tc_execute(0, 1, &run, nullptr) || run.passed != 1) {
        std::printf("SMOKE_CXL: FAIL testcase run: %s\n", run.message);
        return 1;
    }
    std::printf("SMOKE_CXL: ALL PASS (proxy + perf + %u testcases)\n",
                cxl_tlm::cxl_tc_get_count());
    return 0;
}
