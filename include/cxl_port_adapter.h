// -----------------------------------------------------------------------------
// cxl_port_adapter.h -- per-port adapter and swdn backend contract (M2.5).
//
//   Two profiles (see md/ for the customer swdn stand-in specification):
//     TLM_STUB  : CxlTlmStubBackend (functional L-B model, tests only)
//     RTL_SWDN  : customer swdn RTL through the port-adapter wrapper / DPI
//                 bridge (M6; not available in this build)
//
//   The adapter owns the channel init/credit state; the backend owns everything
//   below the customer swdn app interface.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_channel.h"
#include "cxl_cfg_space.h"
#include "cxl_hdm.h"
#include "cxl_mailbox.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cxl_tlm {

enum class CxlBackendKind : std::uint8_t { TLM_STUB = 0, RTL_SWDN = 1 };

const char* cxl_backend_name(CxlBackendKind k);

struct CxlSwdnBackend {
    virtual ~CxlSwdnBackend() = default;
    virtual CxlBackendKind kind() const = 0;
    virtual void set_link(bool up) = 0;
    virtual bool link() const = 0;
    virtual bool cfg_read(std::uint16_t off, unsigned size, std::uint32_t* v,
                          std::string* err = nullptr) = 0;
    virtual bool cfg_write(std::uint16_t off, unsigned size, std::uint32_t val,
                           std::string* err = nullptr) = 0;
    virtual bool mem_read(std::uint64_t dpa, void* dst, std::size_t n,
                          std::string* err = nullptr) = 0;
    virtual bool mem_write(std::uint64_t dpa, const void* src, std::size_t n,
                           std::string* err = nullptr) = 0;
    virtual CxlMailbox* mailbox() = 0;
    virtual std::uint64_t errors() const = 0;
};

// L-B functional stand-in (tests only; NEVER the product profile).
struct CxlTlmStubBackend : CxlSwdnBackend {
    CxlCfgSpace cfg;      // Type0 device
    CxlHdm hdm;           // device HDM (HPA -> DPA)
    CxlMailbox mbx;       // device mailbox (M4 fills commands)
    std::vector<std::uint8_t> mem;   // DPA memory
    bool up = false;
    std::uint64_t error_count = 0;

    void init(std::uint64_t mem_bytes = 16ull << 20);

    CxlBackendKind kind() const override { return CxlBackendKind::TLM_STUB; }
    void set_link(bool v) override { up = v; }
    bool link() const override { return up; }
    bool cfg_read(std::uint16_t off, unsigned size, std::uint32_t* v,
                  std::string* err) override;
    bool cfg_write(std::uint16_t off, unsigned size, std::uint32_t val,
                   std::string* err) override;
    bool mem_read(std::uint64_t dpa, void* dst, std::size_t n,
                  std::string* err) override;
    bool mem_write(std::uint64_t dpa, const void* src, std::size_t n,
                   std::string* err) override;
    CxlMailbox* mailbox() override { return &mbx; }
    std::uint64_t errors() const override { return error_count; }
};

// RTL profile placeholder: the real backend needs the customer swdn + DPI
// bridge (M6).  It deliberately refuses to run instead of pretending.
struct CxlRtlSwdnBackend : CxlSwdnBackend {
    CxlMailbox dummy;
    CxlBackendKind kind() const override { return CxlBackendKind::RTL_SWDN; }
    void set_link(bool) override {}
    bool link() const override { return false; }
    bool cfg_read(std::uint16_t, unsigned, std::uint32_t*, std::string* err) override {
        if (err) *err = "RTL swdn backend requires the DPI bridge (M6)";
        return false;
    }
    bool cfg_write(std::uint16_t, unsigned, std::uint32_t, std::string* err) override {
        if (err) *err = "RTL swdn backend requires the DPI bridge (M6)";
        return false;
    }
    bool mem_read(std::uint64_t, void*, std::size_t, std::string* err) override {
        if (err) *err = "RTL swdn backend requires the DPI bridge (M6)";
        return false;
    }
    bool mem_write(std::uint64_t, const void*, std::size_t, std::string* err) override {
        if (err) *err = "RTL swdn backend requires the DPI bridge (M6)";
        return false;
    }
    CxlMailbox* mailbox() override { return &dummy; }
    std::uint64_t errors() const override { return 0; }
};

struct CxlPortAdapter {
    int index = 0;
    CxlBackendKind profile = CxlBackendKind::TLM_STUB;
    std::unique_ptr<CxlSwdnBackend> backend;

    // Channel state (per channel; v1 uses mem/cache init pairs).
    CxlTxInit tx_init;
    CxlRxInit rx_init;
    CxlCreditPool tx_credits;              // credits granted by the peer
    CxlRxCreditReturn rx_credits;          // credits returned to the peer
    bool app_pending = false;

    void init(int idx);
    bool select_backend(CxlBackendKind k, std::string* err = nullptr);
    CxlBackendKind active_profile() const { return backend ? backend->kind() : profile; }

    bool link() const { return backend && backend->link(); }
    bool cfg_read(std::uint16_t off, unsigned size, std::uint32_t* v,
                  std::string* err = nullptr);
    bool cfg_write(std::uint16_t off, unsigned size, std::uint32_t val,
                   std::string* err = nullptr);
    bool mem_read(std::uint64_t dpa, void* dst, std::size_t n, std::string* err = nullptr);
    bool mem_write(std::uint64_t dpa, const void* src, std::size_t n,
                   std::string* err = nullptr);
};

} // namespace cxl_tlm
