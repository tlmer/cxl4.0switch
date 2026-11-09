// -----------------------------------------------------------------------------
// cxl_ras.h -- CXL RAS Capability Structure (live register block, model side).
//
//   The CPER builder in cxl_cper.h serializes a snapshot of the RAS registers;
//   this header models the registers themselves so an endpoint/port can report
//   errors, apply masks/severity, capture the header log and expose the
//   first-error pointer exactly as an OS driver expects.
//
//   Layout (CXL 2.0 8.2.5.9, length 0x58; offsets from the RAS capability):
//     +00 Uncorrectable Error Status   (RW1C) defined bits 16:14 | 11:0
//     +04 Uncorrectable Error Mask     (RW)   defined bits 16:14 | 11:0
//     +08 Uncorrectable Error Severity (RW)   defined bits 16:14 | 11:0
//     +0C Correctable Error Status     (RW1C) defined bits 6:0
//     +10 Correctable Error Mask       (RW)   defined bits 6:0
//     +14 Error Capability and Control (RO)   [5:0] first-error pointer
//     +18 Header Log (64 bytes = 16 dwords, RO)
//
//   Rules implemented:
//     - a masked status bit still latches; the summary (status & ~mask) is
//       what a driver would report, and fully-masked events are counted;
//     - RW1C writes clear only the bits written as 1;
//     - writes outside the defined bits are masked (never widened);
//     - writes to RO registers/offsets are rejected and counted, not silent.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_cper.h"

#include <cstdint>

namespace cxl_tlm {

struct CxlRasCapability {
    static constexpr std::uint32_t k_uncor_bits = 0x0001CFFFu; // 16:14|11:0
    static constexpr std::uint32_t k_cor_bits = 0x0000007Fu;   // 6:0
    static constexpr std::uint16_t k_size = 0x58;

    std::uint32_t uncor_status = 0;
    std::uint32_t uncor_mask = 0;
    std::uint32_t uncor_severity = 0;
    std::uint32_t cor_status = 0;
    std::uint32_t cor_mask = 0;
    std::uint32_t cap_control = 0;      // [5:0] = first-error pointer (RO)
    std::uint32_t header_log[16] = {};

    std::uint64_t uncor_events = 0, cor_events = 0;
    std::uint64_t uncor_masked_events = 0, cor_masked_events = 0;
    std::uint64_t header_captures = 0, ro_writes = 0, bad_accesses = 0;

    void reset() {
        uncor_status = uncor_mask = uncor_severity = 0;
        cor_status = cor_mask = cap_control = 0;
        for (std::uint32_t& w : header_log) w = 0;
        uncor_events = cor_events = 0;
        uncor_masked_events = cor_masked_events = 0;
        header_captures = ro_writes = bad_accesses = 0;
    }

    // Latch an uncorrectable error (defined bits only).  Zero or undefined
    // bits are rejected rather than silently ignored.
    bool set_uncorrectable(std::uint32_t bits) {
        bits &= k_uncor_bits;
        if (bits == 0) {
            ++bad_accesses;
            return false;
        }
        const std::uint32_t before =
            uncor_status & ~uncor_mask & k_uncor_bits;
        uncor_status |= bits;
        ++uncor_events;
        const std::uint32_t active = uncor_status & ~uncor_mask & k_uncor_bits;
        if (active == 0) {
            ++uncor_masked_events;
        } else if (before == 0) {
            // First unmasked error since the status was last cleared: latch
            // the first-error pointer.  Later errors keep the original one.
            cap_control = (cap_control & ~0x3Fu) | first_bit(bits & ~uncor_mask);
        }
        return true;
    }

    bool set_correctable(std::uint32_t bits) {
        bits &= k_cor_bits;
        if (bits == 0) {
            ++bad_accesses;
            return false;
        }
        cor_status |= bits;
        ++cor_events;
        if ((cor_status & ~cor_mask & k_cor_bits) == 0)
            ++cor_masked_events;
        return true;
    }

    std::uint32_t uncorrectable_summary() const {
        return uncor_status & ~uncor_mask & k_uncor_bits;
    }
    std::uint32_t correctable_summary() const {
        return cor_status & ~cor_mask & k_cor_bits;
    }
    bool severity_fatal(unsigned bit) const {
        return bit < 32 && (uncor_severity & ((std::uint32_t)1u << bit)) != 0;
    }
    // First-error pointer stored in cap_control (bit index); 0 may also mean
    // "no error", so callers pair it with the summary.
    std::uint8_t first_error_pointer() const {
        return (std::uint8_t)(cap_control & 0x3Fu);
    }

    // Snapshot a TLP/protocol header into the 64-byte header log.  The whole
    // 64-byte log is rewritten (unwritten dwords become zero), matching a
    // hardware snapshot of a single failing transaction.
    bool capture_header(const std::uint32_t* hdr, unsigned dwords) {
        if (!hdr || dwords == 0) {
            ++bad_accesses;
            return false;
        }
        if (dwords > 16) dwords = 16;
        for (unsigned i = 0; i < 16; ++i)
            header_log[i] = (i < dwords) ? hdr[i] : 0;
        ++header_captures;
        return true;
    }

    // Snapshot in the CPER Protocol Error section layout (cxl_cper.h).  This
    // is the exact byte image an OS would copy into the error record.
    CxlCperRasRegs cper_regs() const {
        CxlCperRasRegs r;
        r.uncor_status = uncor_status;
        r.uncor_mask = uncor_mask;
        r.uncor_severity = uncor_severity;
        r.cor_status = cor_status;
        r.cor_mask = cor_mask;
        r.cap_control = cap_control;
        for (unsigned i = 0; i < 16; ++i) r.header_log[i] = header_log[i];
        return r;
    }

    bool read32(std::uint16_t off, std::uint32_t* v) const {
        if (!v) {
            return false;
        }
        if (off == 0x00) { *v = uncor_status; return true; }
        if (off == 0x04) { *v = uncor_mask; return true; }
        if (off == 0x08) { *v = uncor_severity; return true; }
        if (off == 0x0C) { *v = cor_status; return true; }
        if (off == 0x10) { *v = cor_mask; return true; }
        if (off == 0x14) { *v = cap_control; return true; }
        if (off >= 0x18 && off < 0x18 + 64 && ((off - 0x18) & 3u) == 0) {
            *v = header_log[(off - 0x18) / 4u];
            return true;
        }
        return false;
    }

    // Register write.  Status registers are RW1C; mask/severity are RW with
    // defined-bit masks; cap_control and the header log are read-only.
    bool write32(std::uint16_t off, std::uint32_t v) {
        switch (off) {
        case 0x00:
            uncor_status &= ~(v & k_uncor_bits);
            return true;
        case 0x04:
            uncor_mask = v & k_uncor_bits;
            return true;
        case 0x08:
            uncor_severity = v & k_uncor_bits;
            return true;
        case 0x0C:
            cor_status &= ~(v & k_cor_bits);
            return true;
        case 0x10:
            cor_mask = v & k_cor_bits;
            return true;
        case 0x14:
        default:
            if (off >= 0x18 && off < 0x18 + 64 && ((off - 0x18) & 3u) == 0) {
                ++ro_writes;
                return false;
            }
            if (off == 0x14) {
                ++ro_writes;
                return false;
            }
            ++bad_accesses;
            return false;
        }
    }

private:
    static std::uint8_t first_bit(std::uint32_t v) {
        std::uint8_t i = 0;
        while (i < 32 && ((v >> i) & 1u) == 0) ++i;
        return i;
    }
};

static_assert(CxlRasCapability::k_size == sizeof(CxlCperRasRegs),
              "CXL RAS register block and CPER RAS image must be 0x58 bytes");

} // namespace cxl_tlm
