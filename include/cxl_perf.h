// -----------------------------------------------------------------------------
// cxl_perf.h -- lightweight performance statistics (checklist #4).
//
//   Measures the model's own operation path (CXL.mem tasks and configuration
//   accesses), not the fabric link bandwidth.  The clock is std::chrono's
//   steady clock; in a simulator host this is wall-clock time, so the numbers
//   are useful for relative regression, not for RTL cycle claims.
// -----------------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <string>

namespace cxl_tlm {

struct CxlPerfSnapshot {
    std::uint64_t ops = 0;
    std::uint64_t bytes = 0;
    std::uint64_t window_ns = 0;
    std::uint64_t latency_min_ns = 0;
    std::uint64_t latency_avg_ns = 0;
    std::uint64_t latency_max_ns = 0;
    double kb_per_s = 0.0;
    double kops_per_s = 0.0;
};

struct CxlPerfWindow {
    std::uint64_t ops = 0;
    std::uint64_t bytes = 0;
    std::uint64_t latency_sum_ns = 0;
    std::uint64_t latency_min_ns = 0;
    std::uint64_t latency_max_ns = 0;
    std::uint64_t start_ns = 0;
    bool started = false;

    void reset();
    void start();
    void record(std::uint64_t bytes, std::uint64_t latency_ns);
    CxlPerfSnapshot snapshot() const;
    std::string report(const char* title) const;
};

std::uint64_t cxl_perf_now_ns();

} // namespace cxl_tlm
