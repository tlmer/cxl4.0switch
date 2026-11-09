// -----------------------------------------------------------------------------
// cxl_dpa.h -- Dynamic Power Allocation (DPA) data-plane model.
//
//   Complements the DPA extended-capability registers (Capability substates,
//   Control/Status) with the substate transition behavior:
//     - a Control write selects a substate below the advertised count;
//     - the transition is pending for `latency` ticks;
//     - Status then reports the new substate; transitions to an invalid
//       substate are rejected and counted.
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>

namespace cxl_tlm {

struct CxlDpa {
    std::uint8_t substates = 8;          // advertised count (1..32)
    std::uint8_t control_substate = 0;
    std::uint8_t status_substate = 0;
    bool transition_pending = false;
    std::uint32_t latency = 2;
    std::uint32_t countdown = 0;

    std::uint64_t transitions = 0, completed = 0, rejected = 0;

    bool write_control(std::uint32_t v) {
        const std::uint8_t target = (std::uint8_t)(v & 0x1Fu);
        if (substates == 0 || target >= substates) { ++rejected; return false; }
        control_substate = target;
        if (target == status_substate) return true;   // no transition needed
        transition_pending = true;
        countdown = latency ? latency : 1;
        ++transitions;
        return true;
    }

    void tick() {
        if (!transition_pending) return;
        if (countdown > 0) --countdown;
        if (countdown == 0) {
            transition_pending = false;
            status_substate = control_substate;
            ++completed;
        }
    }

    std::uint32_t capability() const {
        return (std::uint32_t)((substates ? substates - 1 : 0) & 0x1Fu) | (1u << 5);
    }
    std::uint32_t status() const {
        return (std::uint32_t)(status_substate & 0x1Fu) |
               (transition_pending ? (1u << 5) : 0u);
    }
};

} // namespace cxl_tlm
