// -----------------------------------------------------------------------------
// cxl_acpi.h -- Linux-view CEDT (CHBS/CFMWS) description of the virtual root.
//
//   This is a model view, not a binary ACPI table: the Linux proxy uses the
//   same root complex / HDM regions that the enumerator programs, so its
//   CHBS/CFMWS output stays consistent with the PCIe tree and lspci.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_hdm_prog.h"
#include "cxl_root_complex.h"
#include "cxl_switch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlCfmwsWindow {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::uint32_t interleave_ways = 1;
    std::uint32_t interleave_granularity = 256;   // bytes
    std::uint16_t target_uid = 0;                 // root port/host bridge UID
    std::uint8_t restrictions = 0;
};

struct CxlChbsEntry {
    std::uint16_t uid = 0;
    std::uint8_t bus = 0, dev = 0, fn = 0;        // host bridge BDF
    std::uint8_t bus_base = 0, bus_end = 0;       // root bus range
    std::uint8_t cxl_version = 2;
};

struct CxlCedtView {
    std::vector<CxlChbsEntry> chbs;
    std::vector<CxlCfmwsWindow> cfmws;
};

CxlCedtView cxl_build_cedt(const CxlRootComplex& rc, const CxlSwitch& sw,
                           const std::vector<CxlHdmRegion>& regions);
std::string cxl_cedt_view(const CxlCedtView& cedt);

} // namespace cxl_tlm
