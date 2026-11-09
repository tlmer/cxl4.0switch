// -----------------------------------------------------------------------------
// cxl_mriov.h -- MR-IOV multi-root fabric data-plane model.
//
//   The PCIe MR-IOV capability registers live in the config-space layer
//   (`pcie_ext::mriov()` + CxlCfgSpace).  This header adds the data plane the
//   register face alone cannot provide:
//
//     * up to 8 virtual hierarchies (VH), each with its own memory window;
//     * ports attached to one VH with a per-VH base/size decoder;
//     * route(vh, addr) -> port + translated offset;
//     * per-port, per-VH backing memory, so identical addresses in different
//       VHs are isolated;
//     * explicit no-route / disabled-VH / overlapping-port errors.
//
//   Scope: this is a stand-alone fabric model used by the MRIOV directed
//   bench.  The single-root SystemC switch (`CxlScSwitch`) does not yet carry
//   a VH field on each TLP, so it continues to use the single-root routing
//   path; wiring VH IDs through the private-port contract is a customer
//   config item (the model keeps the register face and this data plane ready).
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace cxl_tlm {

struct CxlMriovVh {
    bool enabled = false;
    std::uint16_t vh_id = 0;
    std::uint64_t mem_base = 0;
    std::uint64_t mem_size = 0;
};

struct CxlMriovPort {
    int id = 0;
    std::uint16_t vh_id = 0;
    std::uint64_t mem_base = 0;
    std::uint64_t mem_size = 0;
    std::uint8_t bus_base = 0;
    std::vector<std::uint8_t> mem;      // per-VH port backing store
};

struct CxlMriovFabric {
    static constexpr unsigned kVh = 8;

    std::array<CxlMriovVh, kVh> vh{};
    std::vector<CxlMriovPort> ports;

    std::uint64_t writes = 0, reads = 0, no_route = 0, disabled = 0;
    std::uint64_t overlap = 0, bad_vh = 0;

    bool add_vh(std::uint16_t vh_id, std::uint64_t base, std::uint64_t size);
    bool disable_vh(std::uint16_t vh_id);
    bool add_port(int id, std::uint16_t vh_id, std::uint64_t base,
                  std::uint64_t size, std::uint8_t bus_base = 0);

    // Route a 64-byte-line access.  Returns the destination port id and the
    // address translated into the port's own window; -1 = no route.
    int route(std::uint16_t vh_id, std::uint64_t addr,
              std::uint64_t* port_addr);

    bool write(std::uint16_t vh_id, std::uint64_t addr, const void* data,
               std::size_t len);
    bool read(std::uint16_t vh_id, std::uint64_t addr, void* data,
              std::size_t len);
};

} // namespace cxl_tlm
