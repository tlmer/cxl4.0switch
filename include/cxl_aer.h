// -----------------------------------------------------------------------------
// cxl_aer.h -- per-component AER (Advanced Error Reporting) injection model.
//
//   The AER extended capability registers live in CxlCfgSpace at 0x300; this
//   header holds the live status/header-log state so every component (USP,
//   DSP, endpoint stand-in) can inject and report errors consistently:
//
//     +04 Uncorrectable Error Status   RW1C
//     +08 Uncorrectable Error Mask     RW
//     +0C Uncorrectable Error Severity RW
//     +10 Correctable Error Status     RW1C
//     +14 Correctable Error Mask       RW
//     +18 Advanced Error Cap/Control   RW bits [6:0]
//     +1C..+2B Header Log (4 DW)       RO
//
//   Injection: a status bit is latched even when masked (the mask affects the
//   reported summary); the first unmasked error captures the header log and
//   the First Error Pointer in Cap/Control.  RW1C writes clear only the bits
//   written as one.
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>

namespace cxl_tlm {

struct CxlAer {
    std::uint32_t uncor_status = 0;
    std::uint32_t uncor_mask = 0;
    std::uint32_t uncor_severity = 0;
    std::uint32_t cor_status = 0;
    std::uint32_t cor_mask = 0;
    std::uint32_t cap_control = 0;                    // [4:0] first error ptr
    std::array<std::uint32_t, 4> header_log{};

    std::uint64_t uncor_events = 0, cor_events = 0, masked_events = 0;
    std::uint64_t header_captures = 0, ro_writes = 0, bad_accesses = 0;

    // Inject an uncorrectable error set.  The first bit becomes the First
    // Error Pointer if the error is not masked; the header log is captured
    // only for the first unmasked error.
    bool inject_uncorrectable(std::uint32_t bits,
                              const std::uint32_t* hdr = nullptr) {
        if (bits == 0) { ++bad_accesses; return false; }
        uncor_status |= bits;
        ++uncor_events;
        if ((bits & ~uncor_mask) == 0) { ++masked_events; return true; }
        if ((cap_control & 0x1Fu) == 0 || header_log[0] == 0) {
            std::uint8_t first = 0;
            while (first < 32 && ((bits >> first) & 1u) == 0) ++first;
            cap_control = (cap_control & ~0x1Fu) | (first & 0x1Fu);
            if (hdr) {
                for (int i = 0; i < 4; ++i) header_log[(std::size_t)i] = hdr[i];
                ++header_captures;
            }
        }
        return true;
    }

    bool inject_correctable(std::uint32_t bits) {
        if (bits == 0) { ++bad_accesses; return false; }
        cor_status |= bits;
        ++cor_events;
        if ((bits & ~cor_mask) == 0) ++masked_events;
        return true;
    }

    bool uncorrectable_pending() const {
        return (uncor_status & ~uncor_mask) != 0;
    }
    bool correctable_pending() const {
        return (cor_status & ~cor_mask) != 0;
    }

    bool read32(std::uint16_t rel, std::uint32_t* v) const {
        if (!v) return false;
        switch (rel) {
        case 0x04: *v = uncor_status; return true;
        case 0x08: *v = uncor_mask; return true;
        case 0x0C: *v = uncor_severity; return true;
        case 0x10: *v = cor_status; return true;
        case 0x14: *v = cor_mask; return true;
        case 0x18: *v = cap_control; return true;
        default: break;
        }
        if (rel >= 0x1C && rel < 0x2C) {
            *v = header_log[(rel - 0x1C) / 4];
            return true;
        }
        return false;
    }

    bool write32(std::uint16_t rel, std::uint32_t v) {
        switch (rel) {
        case 0x04: uncor_status &= ~v; return true;       // RW1C
        case 0x08: uncor_mask = v; return true;
        case 0x0C: uncor_severity = v; return true;
        case 0x10: cor_status &= ~v; return true;         // RW1C
        case 0x14: cor_mask = v; return true;
        case 0x18: cap_control = v & 0x63u; return true;
        default: break;
        }
        if (rel >= 0x1C && rel < 0x2C) { ++ro_writes; return false; }
        ++bad_accesses;
        return false;
    }
};

} // namespace cxl_tlm
