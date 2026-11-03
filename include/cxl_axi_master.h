// -----------------------------------------------------------------------------
// cxl_axi_master.h -- 128-bit AXI4 master used as the model's motherboard DDR
// port.
//
//   The CXL model owns the host side: root port, switch USP/crossbar and the
//   host-memory view.  On a real board that host memory is DDR behind an AXI4
//   interconnect, not a C++ vector.  This file defines the model-side AXI4
//   master and a pluggable backend:
//
//     model (CxlRoot / CxlCacheHome) --CxlAxiMaster--> CxlAxiBackend
//                                                       |
//                        +------------------------------+-------------------+
//                        |                                                  |
//              CxlAxiSramBackend                                DPI backend (cosim)
//              (functional reference, model tests)              (SV bridge -> axi_ram RTL)
//
//   Wire contract (same shape as the family's DPI bridge):
//     data width = 128 bit, address width = 64 bit, AxID = 4 bit,
//     outstanding = 8, max burst = 256 beats (4 KiB).
//     AXI4 INCR bursts; a transfer that is not 16-byte aligned is aligned
//     down and byte strobes select the requested lanes.
//
//   Usage:
//     CxlAxiMaster axi;
//     CxlAxiSramBackend sram;             // or a DPI-backed CxlAxiBackend
//     sram.init(0, 64ull << 20);
//     axi.init(&sram);
//     axi.write(0x1000, data, 64);        // model -> external DDR
//     axi.read (0x1000, data, 64);        // external DDR -> model
//
//   The split-phase submit/poll API is available for callers that need several
//   transactions in flight; the blocking read/write helpers are for functional
//   model code.
// -----------------------------------------------------------------------------
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cxl_tlm {

// AXI4 channel response codes (BRESP/RRESP).
enum class CxlAxiResp : std::uint8_t {
    OKAY = 0,
    EXOKAY = 1,
    SLVERR = 2,
    DECERR = 3
};
const char* cxl_axi_resp_name(CxlAxiResp r);

constexpr unsigned kCxlAxiDataBytes = 16;       // 128-bit data
constexpr unsigned kCxlAxiDataWidth = 128;
constexpr unsigned kCxlAxiAddrWidth = 64;
constexpr unsigned kCxlAxiIdWidth = 4;
constexpr unsigned kCxlAxiOutstanding = 8;
constexpr unsigned kCxlAxiMaxBeats = 256;       // 4 KiB at 16 B/beat

// One AXI burst, expressed in 16-byte beats.
struct CxlAxiTxn {
    bool write = false;
    std::uint64_t addr = 0;          // 16-byte aligned burst base
    unsigned beats = 0;              // 1..kCxlAxiMaxBeats
    unsigned size = 4;               // AxSIZE: log2(bytes/beat) == 4 for 128 bit
    unsigned id = 0;                 // AxID, 4 bit
    std::vector<std::uint8_t> wdata; // beats*16 bytes (write)
    std::vector<std::uint8_t> wstrb; // beats*16 bytes, one strobe per byte
    std::vector<std::uint8_t> rdata; // beats*16 bytes (read, filled by backend)
};

struct CxlAxiResult {
    CxlAxiResp resp = CxlAxiResp::OKAY;
    unsigned beats = 0;
    std::vector<std::uint8_t> rdata; // read payload when resp == OKAY
    std::string err;
};

enum class CxlAxiAccept : std::uint8_t {
    ACCEPTED = 0,   // request queued
    RETRY = 1,      // backend full; caller may retry
    ERROR = 2       // request rejected (bad shape / backend error)
};

enum class CxlAxiPoll : std::uint8_t {
    PENDING = 0,    // not completed yet
    DONE = 1,       // completion returned
    ERROR = 2       // unknown handle / backend error
};

// A backend executes AXI bursts.  The functional SRAM backend runs in-process;
// the DPI backend hands requests to an SV AXI4 master engine.
struct CxlAxiBackend {
    virtual ~CxlAxiBackend() = default;
    virtual const char* name() const = 0;

    // Reset internal queues.  The external memory contents are not cleared by
    // the model; reset of the DDR is the testbench/platform's responsibility.
    virtual void reset() = 0;

    virtual CxlAxiAccept submit(const CxlAxiTxn& txn, std::uint64_t* handle,
                                std::string* err = nullptr) = 0;
    virtual CxlAxiPoll poll(std::uint64_t handle, CxlAxiResult* result,
                            std::string* err = nullptr) = 0;
};

// ---------------------------------------------------------------------------
// Functional external-SRAM reference backend.
//
//   `base`/`size` describe the DDR window seen on the AXI bus.  A burst that
//   leaves the window completes with DECERR (the model never silently drops
//   it).  `latency` is the number of poll() calls before the burst retires;
//   it is only a simulation knob.
// ---------------------------------------------------------------------------
struct CxlAxiSramBackend : CxlAxiBackend {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    unsigned latency = 1;
    unsigned max_outstanding = kCxlAxiOutstanding;
    std::vector<std::uint8_t> mem;

    std::uint64_t wr_bursts = 0, rd_bursts = 0;
    std::uint64_t wr_beats = 0, rd_beats = 0;
    std::uint64_t wr_bytes = 0, rd_bytes = 0;
    std::uint64_t decerr = 0, shape_errors = 0, completed = 0;

    bool init(std::uint64_t window_base, std::uint64_t window_size,
              std::string* err = nullptr);

    // Test / platform hooks.  `poke` writes the DDR directly (bypasses AXI);
    // `peek` reads it back, so tests can prove that traffic really crossed the
    // bus instead of a shadow buffer.
    bool poke(std::uint64_t addr, const void* src, std::size_t n,
              std::string* err = nullptr);
    bool peek(std::uint64_t addr, void* dst, std::size_t n,
              std::string* err = nullptr) const;

    const char* name() const override { return "cxl_axi_sram"; }
    void reset() override;
    CxlAxiAccept submit(const CxlAxiTxn& txn, std::uint64_t* handle,
                        std::string* err = nullptr) override;
    CxlAxiPoll poll(std::uint64_t handle, CxlAxiResult* result,
                    std::string* err = nullptr) override;

private:
    struct Pending {
        std::uint64_t handle = 0;
        CxlAxiTxn txn;
        unsigned cycles_left = 0;
    };
    std::vector<Pending> pending_;
    std::map<std::uint64_t, CxlAxiResult> completed_;
    std::uint64_t next_handle_ = 1;
};

// ---------------------------------------------------------------------------
// Model AXI4 master.
// ---------------------------------------------------------------------------
struct CxlAxiMaster {
    CxlAxiBackend* backend = nullptr;
    unsigned outstanding_limit = kCxlAxiOutstanding;
    unsigned id = 0;

    std::uint64_t submitted = 0, completed = 0;
    std::uint64_t write_bytes = 0, read_bytes = 0;
    std::uint64_t write_beats = 0, read_beats = 0;
    std::uint64_t errors = 0, decerr = 0, retries = 0, timeouts = 0;
    std::uint64_t sync_spins = 0;

    bool init(CxlAxiBackend* be, std::string* err = nullptr);
    void reset();
    bool attached() const { return backend != nullptr; }

    // Blocking helpers.  They are intended for the functional model thread.
    // `timeout_spins` bounds the polling loop so a broken backend cannot hang
    // the model forever.
    bool write(std::uint64_t addr, const void* src, std::size_t n,
               std::string* err = nullptr, unsigned timeout_spins = 1000000);
    bool write_strobes(std::uint64_t addr, const void* src,
                       const std::vector<std::uint8_t>& byte_mask,
                       std::string* err = nullptr, unsigned timeout_spins = 1000000);
    bool read(std::uint64_t addr, void* dst, std::size_t n,
              std::string* err = nullptr, unsigned timeout_spins = 1000000);

    // Split-phase API.
    bool submit_write(std::uint64_t addr, const void* src, std::size_t n,
                      std::uint64_t* handle, std::string* err = nullptr);
    bool submit_write_strobes(std::uint64_t addr, const void* src,
                              const std::vector<std::uint8_t>& byte_mask,
                              std::uint64_t* handle, std::string* err = nullptr);
    bool submit_read(std::uint64_t addr, std::size_t n,
                     std::uint64_t* handle, std::string* err = nullptr);
    CxlAxiPoll poll(std::uint64_t handle, CxlAxiResult* result,
                    std::string* err = nullptr);
    bool complete(std::uint64_t handle, void* dst, std::size_t n,
                  CxlAxiResult* result = nullptr, std::string* err = nullptr,
                  unsigned timeout_spins = 1000000);

private:
    struct Slot {
        bool used = false;
        bool write = false;
        std::uint64_t handle = 0;
        std::uint64_t user_addr = 0;   // byte address requested by the caller
        std::size_t bytes = 0;         // caller payload bytes
        unsigned id = 0;
    };
    std::vector<Slot> slots_;
    std::uint64_t next_handle_ = 1;

    bool submit_txn(const CxlAxiTxn& txn, std::uint64_t user_addr,
                    std::size_t user_n, std::uint64_t* handle,
                    std::string* err);
    bool build_txn(bool write, std::uint64_t addr, const void* src,
                   std::size_t n, const std::vector<std::uint8_t>* byte_mask,
                   CxlAxiTxn* txn, std::string* err);
    Slot* find_slot(std::uint64_t handle);
};

} // namespace cxl_tlm
