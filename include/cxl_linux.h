// -----------------------------------------------------------------------------
// cxl_linux.h -- no-OS Linux behavior proxy (M4.6/M4.7).
//
//   Sequence (design Sec. 4.8):
//     ACPI/CEDT view -> PCI/CXL discovery -> HDM programming -> region ->
//     mailbox (Identify/Health) -> memory task -> events -> poison -> PM.
//
//   This is a model of the *behavior* a Linux CXL stack performs; it does not
//   claim to be the kernel.  All steps log under [ACPI]/[ENUM]/[HDM]/[MBX]/
//   [MEM]/[REGION]/[EVT]/[POISON]/[PM].
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_commands.h"
#include "cxl_dts.h"
#include "cxl_enum.h"
#include "cxl_hdm_prog.h"
#include "cxl_mem.h"
#include "cxl_perf.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlLinuxProxy {
    CxlMemSystem sys;
    std::vector<CxlType3Commands> dev;
    CxlDeviceTree tree;
    std::string log;
    bool ready = false;
    CxlPerfWindow perf_mem;
    CxlPerfWindow perf_cfg;

    struct Region { int port = 0; std::uint64_t base = 0; std::uint64_t size = 0; };
    std::vector<Region> regions;
    std::vector<std::uint64_t> capacity_256m;

    bool init(int n = 4, std::string* err = nullptr);
    bool init(const CxlDtsConfig& dts, std::string* err = nullptr);

    bool acpi_view();
    bool discover();
    bool program_hdm();
    bool create_regions();
    bool mailbox_probe(int port);
    bool mem_task(int port);
    bool event_roundtrip(int port);
    bool poison_roundtrip(int port);
    bool shutdown();
    bool run_all();

    std::string cover() const;
    bool console(const std::string& line, std::string* out);
};

} // namespace cxl_tlm
