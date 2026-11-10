// -----------------------------------------------------------------------------
// cxl_cpmu_msix.h -- optional CPMU overflow -> MSI-X/IRQ bridge.
//
//   CxlCpmu already exposes the MSI_N vector and an `irq_hook` function
//   pointer.  This helper binds that hook to a concrete raise path:
//     * generic owner callback (unit tests, platform IRQ bridge);
//     * CxlMsixTable::trigger_and_raise (table entry vector = MSI_N).
//
//   The bridge never invents a successful raise: a refusing hook is counted
//   as blocked by CxlCpmu, and an unbound hook is counted as unrouted.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_cpmu.h"
#include "cxl_msix_table.h"

#include <cstdint>

namespace cxl_tlm {

struct CxlCpmuMsixBridge {
    using RaiseFn = bool (*)(void* ctx, std::uint32_t vector);

    CxlCpmu* cpmu = nullptr;
    RaiseFn raise = nullptr;
    void* raise_ctx = nullptr;
    std::uint64_t calls = 0;
    std::uint64_t accepted = 0;
    std::uint64_t rejected = 0;

    static bool thunk(void* ctx, std::uint32_t vector) {
        CxlCpmuMsixBridge* b = static_cast<CxlCpmuMsixBridge*>(ctx);
        if (!b || !b->raise) return false;
        ++b->calls;
        const bool ok = b->raise(b->raise_ctx, vector);
        if (ok) ++b->accepted; else ++b->rejected;
        return ok;
    }

    bool bind(CxlCpmu& unit, RaiseFn fn, void* ctx = nullptr) {
        if (!fn) return false;
        cpmu = &unit;
        raise = fn;
        raise_ctx = ctx;
        unit.irq_hook = &CxlCpmuMsixBridge::thunk;
        unit.irq_hook_ctx = this;
        return true;
    }

    // Direct table binding: the CPMU's MSI_N vector selects the MSI-X table
    // entry; trigger_and_raise also drives the bound host root IRQ table.
    static bool msix_thunk(void* ctx, std::uint32_t vector) {
        CxlMsixTable* table = static_cast<CxlMsixTable*>(ctx);
        return table && table->trigger_and_raise(vector) >= 0;
    }

    bool bind_msix(CxlCpmu& unit, CxlMsixTable& table) {
        return bind(unit, &CxlCpmuMsixBridge::msix_thunk, &table);
    }

    bool raise_pending() {
        return cpmu && cpmu->raise_overflow_irq();
    }
};

} // namespace cxl_tlm
