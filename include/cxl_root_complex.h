// -----------------------------------------------------------------------------
// cxl_root_complex.h -- Linux-like host bridge + root port configuration.
//
//   The CXL root complex is the host-side owner of bus 0:
//     00:00.0  Host bridge  Type0 / class 060000
//     00:01.0  Root port    Type1 / class 060400 / PCIe Port Type 4
//
//   CXL root HDM decoders / host memory / IRQ stay in CxlRoot.  This class is
//   only the PCIe configuration function that lspci/Linux sees at bus 0; the
//   CXL-specific root view (CHBS/CFMWS) is built separately.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_cfg_space.h"

#include <cstdint>
#include <string>

namespace cxl_tlm {

struct CxlBusMap {
    std::uint8_t root_bus = 0;
    std::uint8_t root_port_dev = 1;
    std::uint8_t usp_bus = 1;       // root port secondary bus
    std::uint8_t usp_sec = 2;       // switch internal bus
    std::uint8_t dsp_bus = 2;       // DSP functions live on the internal bus
    std::uint8_t first_ep_bus = 3;  // DSP0 secondary bus
};

struct CxlRootComplex {
    CxlCfgSpace host_bridge;
    CxlCfgSpace root_port;
    CxlBusMap buses;

    void init();
    void reset();

    bool cfg_read(std::uint8_t bus, std::uint8_t dev, std::uint8_t fn,
                  std::uint16_t off, unsigned size, std::uint32_t* val,
                  std::string* err = nullptr) const;
    bool cfg_write(std::uint8_t bus, std::uint8_t dev, std::uint8_t fn,
                   std::uint16_t off, unsigned size, std::uint32_t val,
                   std::string* err = nullptr);

    // Program the Type1 root-port bus numbers and enable Memory/Bus Master.
    bool program_root_port(std::uint8_t sec, std::uint8_t sub,
                           std::string* err = nullptr);
    bool read_root_port_bus(std::uint32_t* bus_reg, std::uint16_t* sub,
                            std::string* err = nullptr) const;
};

} // namespace cxl_tlm
