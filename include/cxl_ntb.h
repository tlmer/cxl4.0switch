// -----------------------------------------------------------------------------
// cxl_ntb.h -- F1a: minimal NTB function block for the CXL switch model.
//
//   Bidirectional translation windows (4), doorbell, scratchpad (8 x 32-bit)
//   and A/B-side memory.  Addresses are absolute in each side's address space:
//     A-side window  [a_base, a_base+size)  <-> B-side [b_base, b_base+size)
//   An A-side access inside a window is translated to the B side (and vice
//   versa).  Doorbell at offset 0 (relative to the NTB base) and scratchpad at
//   [0x200, 0x220) are shared by both sides.  Out-of-window accesses are never
//   silent: reads return an explicit UR-class error, writes are counted and
//   dropped.
// -----------------------------------------------------------------------------
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace cxl_tlm {

struct CxlNtbWindow {
    std::uint64_t a_base = 0, b_base = 0, size = 0;
    bool enabled = false;
    // Symmetric window-pair buffer: both the A-side window and the B-side
    // window translate to this memory (A/B are two views of the same window).
    std::array<std::uint8_t, 4096> mem{};
};

struct CxlNtb {
    static constexpr int WIN_MAX = 4;
    static constexpr std::size_t MEM_BYTES = 4096;
    static constexpr int SP_MAX = 8;
    static constexpr std::uint64_t DOORBELL_OFF = 0x0000;
    static constexpr std::uint64_t SP_OFF_BASE = 0x0200;
    static constexpr std::uint64_t SP_OFF_LIMIT = 0x0220;

    CxlNtbWindow win[WIN_MAX];
    bool enable = true;
    std::array<std::uint8_t, MEM_BYTES> a_mem{};
    std::array<std::uint8_t, MEM_BYTES> b_mem{};
    std::uint32_t sp[SP_MAX] = {0};
    std::uint64_t events = 0;                 // doorbell event count

    // Statistics.
    std::uint64_t win_rd = 0, win_wr = 0, doorbell_wr = 0, doorbell_rd = 0;
    std::uint64_t sp_rd = 0, sp_wr = 0, b_oob = 0;
    std::uint64_t miss_wr = 0, miss_rd_ur = 0, rev_fwd = 0;

    void init();
    void clear_windows();
    bool set_window(int index, std::uint64_t a_base, std::uint64_t b_base,
                    std::uint64_t size, std::string* err = nullptr);

    // side: 0 = A (host), 1 = B (peer).  n <= 8, must stay within one 4-byte
    // aligned unit for this minimal model.
    bool write(int side, std::uint64_t addr, const void* data, std::size_t n,
               std::string* err = nullptr);
    bool read(int side, std::uint64_t addr, void* data, std::size_t n,
              std::string* err = nullptr);

    void ring_doorbell();
    std::uint64_t doorbell() const { return events; }
    bool sp_write(unsigned index, std::uint32_t val, std::string* err = nullptr);
    bool sp_read(unsigned index, std::uint32_t* val, std::string* err) const;
};

} // namespace cxl_tlm
