// -----------------------------------------------------------------------------
// cxl_host_transport.h -- host-side transport used when the Linux proxy drives
// an external DUT (RTL/DPI) instead of the model's local CxlSwitch.
//
//   Configuration traffic is BDF/register based; CXL.mem traffic is HPA based.
//   The local model implementation and the DPI/RTL implementation both satisfy
//   this interface, so the Linux proxy and remote enumerator do not need to
//   know whether the endpoint is local or a DUT.
// -----------------------------------------------------------------------------
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace cxl_tlm {

struct CxlHostTransport {
    virtual ~CxlHostTransport() = default;

    virtual bool cfg_read(std::uint8_t bus, std::uint8_t dev, std::uint8_t fn,
                          std::uint16_t off, unsigned size, std::uint32_t* val,
                          std::string* err = nullptr) = 0;
    virtual bool cfg_write(std::uint8_t bus, std::uint8_t dev, std::uint8_t fn,
                           std::uint16_t off, unsigned size, std::uint32_t val,
                           std::string* err = nullptr) = 0;

    virtual bool mem_read(std::uint64_t hpa, void* dst, std::size_t n,
                          std::string* err = nullptr) = 0;
    virtual bool mem_write(std::uint64_t hpa, const void* src, std::size_t n,
                           std::string* err = nullptr) = 0;

    // Link/init gate exposed by the DUT transport.  The local model returns
    // true immediately; the DPI transport waits for rdlh_link_up and the
    // CXL link-layer init done.
    virtual bool wait_link(unsigned timeout_ms, std::string* err = nullptr) = 0;
};

} // namespace cxl_tlm
