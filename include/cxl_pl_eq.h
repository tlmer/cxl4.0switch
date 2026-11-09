// -----------------------------------------------------------------------------
// cxl_pl_eq.h -- Physical Layer equalization state model (16/32/64 GT/s).
//
//   The PL 16/32/64 GT/s extended capabilities expose per-lane equalization
//   control (preset) and status.  This model runs the equalization sequence:
//
//     IDLE -> START (start()) -> COMPLETE after `phase_ticks` ticks
//                              -> FAILED if the caller reports a failure.
//
//   Status bits: [0] Equalization Complete, [1] Equalization Failed,
//   [2] Link Equalization Request.
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>

namespace cxl_tlm {

struct CxlPlEq {
    enum State : std::uint8_t { IDLE = 0, START = 1, COMPLETE = 2, FAILED = 3 };

    State state = IDLE;
    std::uint32_t lanes = 4;
    std::uint32_t phase_ticks = 4;
    std::uint32_t ticks = 0;
    std::array<std::uint8_t, 16> preset{};
    bool equalization_request = false;

    std::uint64_t starts = 0, completions = 0, failures = 0;

    void start() {
        state = START;
        ticks = 0;
        equalization_request = false;
        ++starts;
    }

    void tick() {
        if (state != START) return;
        if (++ticks >= (phase_ticks ? phase_ticks : 1)) {
            state = COMPLETE;
            ++completions;
        }
    }

    void fail() {
        if (state == START) {
            state = FAILED;
            ++failures;
        }
    }

    bool complete() const { return state == COMPLETE; }

    std::uint32_t status() const {
        std::uint32_t s = 0;
        if (state == COMPLETE) s |= 0x1u;
        if (state == FAILED)   s |= 0x2u;
        if (equalization_request) s |= 0x4u;
        return s;
    }

    bool set_preset(unsigned lane, std::uint8_t value) {
        if (lane >= preset.size() || lane >= lanes) return false;
        preset[lane] = (std::uint8_t)(value & 0xFu);
        return true;
    }
};

} // namespace cxl_tlm
