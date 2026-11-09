// -----------------------------------------------------------------------------
// cxl_npem.h -- Native PCIe Enclosure Management (NPEM) command model.
//
//   NPEM Control bit0 = Enable, bits [11:1] are command/indicator bits (Reset,
//   OK, Locate, Fail, Rebuild, PFA, Hot Spare, ICA, IFA, IDT, Disabled).
//   NPEM Status bit0 = Command Completed.  A command completes after a
//   modeled latency; a new command while one is pending is rejected and
//   counted (never silent).
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>

namespace cxl_tlm {

struct CxlNpem {
    bool enabled = false;
    std::uint32_t control = 0;
    std::uint32_t status = 0;          // bit0 Command Completed
    std::uint32_t latency = 4;         // ticks from command to completion
    std::uint32_t countdown = 0;
    bool pending = false;

    std::uint64_t commands = 0, completed = 0, rejected = 0, inserted = 0;

    // Control write.  Returns false when a command arrives while one is
    // still pending (the write is rejected and counted).
    bool write_control(std::uint32_t v) {
        const bool en = (v & 0x1u) != 0;
        const std::uint32_t cmd = v & 0xFF000FFEu;   // command bits [11:1],[31:24]
        if (cmd != 0 && pending) { ++rejected; return false; }
        enabled = en;
        control = v;
        if (cmd != 0 && en) {
            status &= ~0x1u;                     // Command Completed = 0
            pending = true;
            countdown = latency ? latency : 1;
            ++commands;
        }
        return true;
    }

    void tick() {
        if (!pending) return;
        if (countdown > 0) --countdown;
        if (countdown == 0) {
            pending = false;
            status |= 0x1u;                      // Command Completed
            ++completed;
        }
    }

    bool command_completed() const { return (status & 0x1u) != 0; }
};

} // namespace cxl_tlm
