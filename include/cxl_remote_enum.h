// -----------------------------------------------------------------------------
// cxl_remote_enum.h -- root-first enumeration over an external DUT transport.
//
//   The model owns the virtual host bridge/root port (CxlRootComplex).  The
//   switch USP and everything below it are accessed through CxlHostTransport,
//   which may be a local model switch or a DPI/RTL bridge.  This is the path
//   used to drive a DUT/VIP with the model as host.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_enum.h"
#include "cxl_host_transport.h"
#include "cxl_root_complex.h"

namespace cxl_tlm {

struct CxlUsp;

struct CxlRemoteAttach {
    // Bus/device where the DUT switch USP is reachable before enumeration.
    // W5 SW RTL starts at bus0/dev0; standard Linux then moves it to usp_bus.
    std::uint8_t initial_bus = 0;
    std::uint8_t initial_dev = 0;
    std::uint8_t usp_bus = 1;
    std::uint8_t usp_sec = 2;
    bool move_usp = true;
};

// Enumerate host bridge/root port locally and USP/DSP/EP through `transport`.
// If `model_usp` is non-null, the USP is model-owned and configured locally;
// the transport then carries the DUT DSP (Type1, PCIe cap type 6) and the
// endpoint behind it.  If it is null, the transport carries the DUT USP and
// its DSP/EPs (legacy path).
// The returned tree uses the same CxlEnumDev/CxlEnumRole shape as the local
// enumerator, so lspci/cxl list can use it unchanged.
CxlDeviceTree cxl_enumerate_remote(CxlRootComplex& rc, CxlHostTransport& transport,
                                   const CxlRemoteAttach& attach,
                                   int num_ports,
                                   const CxlEnumConfig& cfg = CxlEnumConfig(),
                                   CxlUsp* model_usp = nullptr);

} // namespace cxl_tlm
