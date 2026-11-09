// -----------------------------------------------------------------------------
// cxl_msix_table.h -- MSI-X table/PBA image in host memory (model side).
//
//   The MSI-X table and Pending Bit Array are memory-mapped registers inside
//   the function's BAR, not configuration space.  This helper models that
//   image on top of CxlRoot host memory so an endpoint model (or a unit test)
//   can program entries, mask vectors, raise interrupts and observe the PBA
//   exactly as an OS would through CXL.io memory writes.
//
//   Table entry (16 bytes, PCIe base spec):
//     +00 Message Address low   32
//     +04 Message Address high  32
//     +08 Message Data          32
//     +0C Vector Control        32  (bit0 = mask; bits 31:1 reserved)
//
//   Pending Bit Array: one bit per vector, packed in 64-bit words; the array
//   size is ceil(nvec / 64) * 8 bytes.
//
//   Semantics implemented:
//     - trigger(): disabled  => no message, counted as disabled;
//                  masked    => the PBA bit is set (pending), no message;
//                  unmasked  => returns the programmed addr/data.
//     - deliver_pending(): after software clears a mask, a pending vector can
//                  be delivered and its PBA bit is cleared (hardware would
//                  send the message at unmask time).
//     - clear_pending() models the software-visible clear path.
//   Nothing is invented on transport failure: a failed host-memory access is
//   counted and returned as an error.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_root.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlMsixTable {
    CxlRoot* root = nullptr;
    std::uint64_t table_base = 0;   // HPA of the table (from Table Offset/BIR)
    std::uint64_t pba_base = 0;     // HPA of the PBA (from PBA Offset/BIR)
    unsigned nvec = 0;
    bool enable = false;            // MSI-X Enable (Message Control bit15)
    bool fn_mask = false;           // Function Mask (Message Control bit14)

    struct Entry {
        std::uint64_t addr = 0;
        std::uint32_t data = 0;
        std::uint32_t ctrl = 0;
        bool masked() const { return (ctrl & 1u) != 0; }
    };

    // Counters: every path is accounted for; failures are never silent.
    std::uint64_t table_writes = 0, table_reads = 0, mask_writes = 0;
    std::uint64_t triggers_sent = 0, triggers_masked = 0;
    std::uint64_t triggers_disabled = 0, triggers_flushed = 0;
    std::uint64_t pba_set = 0, pba_cleared = 0, errors = 0;

    static constexpr unsigned k_max_vectors = 2048; // PCIe maximum

    std::size_t table_bytes() const { return (std::size_t)nvec * 16u; }
    std::size_t pba_bytes() const { return ((std::size_t)nvec + 63u) / 64u * 8u; }

    // Bind to a host-memory image.  The caller must have already programmed
    // the BAR (base + Table/PBA Offset) and sized the region.  Re-binding
    // clears the enable/mask state and all counters.
    bool attach(CxlRoot* r, std::uint64_t tbl, std::uint64_t pba, unsigned n,
                std::string* err = nullptr) {
        root = r;
        table_base = tbl;
        pba_base = pba;
        nvec = n;
        enable = false;
        fn_mask = false;
        table_writes = table_reads = mask_writes = 0;
        triggers_sent = triggers_masked = triggers_disabled = triggers_flushed = 0;
        pba_set = pba_cleared = errors = 0;
        if (!r) {
            if (err) *err = "MSI-X: null host root";
            return false;
        }
        if (n == 0 || n > k_max_vectors) {
            if (err) *err = "MSI-X: vector count out of range";
            return false;
        }
        return true;
    }

    // Program one table entry (what the OS does with a 16-byte memory write).
    bool program_entry(unsigned idx, std::uint64_t addr, std::uint32_t data,
                       std::uint32_t ctrl = 0, std::string* err = nullptr) {
        if (!check_index(idx, err)) return false;
        std::uint8_t b[16] = {};
        std::memcpy(&b[0], &addr, 8);
        std::memcpy(&b[8], &data, 4);
        std::memcpy(&b[12], &ctrl, 4);
        if (!root->mem_write(table_base + (std::uint64_t)idx * 16u, b,
                             sizeof(b), err)) {
            ++errors;
            return false;
        }
        ++table_writes;
        return true;
    }

    bool read_entry(unsigned idx, Entry* e, std::string* err = nullptr) {
        if (!e) {
            if (err) *err = "MSI-X: null entry output";
            ++errors;
            return false;
        }
        if (!check_index(idx, err)) return false;
        std::uint8_t b[16] = {};
        if (!root->mem_read(table_base + (std::uint64_t)idx * 16u, b,
                            sizeof(b), err)) {
            ++errors;
            return false;
        }
        std::memcpy(&e->addr, &b[0], 8);
        std::memcpy(&e->data, &b[8], 4);
        std::memcpy(&e->ctrl, &b[12], 4);
        ++table_reads;
        return true;
    }

    // Vector Control bit0 (per-vector mask).
    bool set_mask(unsigned idx, bool masked, std::string* err = nullptr) {
        Entry e;
        if (!read_entry(idx, &e, err)) return false;
        const std::uint32_t nv = masked ? (e.ctrl | 1u) : (e.ctrl & ~1u);
        if (nv == e.ctrl) return true;
        if (!program_entry(idx, e.addr, e.data, nv, err)) return false;
        ++mask_writes;
        return true;
    }

    // Raise one vector.  Returns true only when a message would be sent.
    bool trigger(unsigned idx, std::uint64_t* addr, std::uint32_t* data,
                 std::string* err = nullptr) {
        if (!check_index(idx, err)) return false;
        if (!enable) {
            ++triggers_disabled;
            if (err) *err = "MSI-X disabled";
            return false;
        }
        Entry e;
        if (!read_entry(idx, &e, err)) return false;
        if (fn_mask || e.masked()) {
            if (!set_pba_bit(idx, true, err)) return false;
            ++triggers_masked;
            if (err) *err = "MSI-X vector masked (pending set)";
            return false;
        }
        if (addr) *addr = e.addr;
        if (data) *data = e.data;
        ++triggers_sent;
        return true;
    }

    // Model the host root's MSI receive path: trigger + IRQ table lookup.
    // Returns the root vector, or -1 when no message was sent / not enrolled.
    int trigger_and_raise(unsigned idx, std::string* err = nullptr) {
        std::uint64_t a = 0;
        std::uint32_t d = 0;
        if (!trigger(idx, &a, &d, err)) return -1;
        return root ? root->raise_msi(a, d) : -1;
    }

    bool pending(unsigned idx, std::string* err = nullptr) {
        if (!check_index(idx, err)) return false;
        std::uint64_t v = 0;
        if (!root->mem_read(pba_base + (std::uint64_t)(idx / 64u) * 8u,
                            &v, sizeof(v), err)) {
            ++errors;
            return false;
        }
        return ((v >> (idx % 64u)) & 1u) != 0;
    }

    bool read_pba(std::vector<std::uint64_t>* out, std::string* err = nullptr) {
        if (!out) {
            if (err) *err = "MSI-X: null PBA output";
            ++errors;
            return false;
        }
        out->assign((std::size_t)((nvec + 63u) / 64u), 0);
        for (std::size_t i = 0; i < out->size(); ++i) {
            if (!root->mem_read(pba_base + (std::uint64_t)i * 8u, &(*out)[i],
                                sizeof((*out)[i]), err)) {
                ++errors;
                return false;
            }
        }
        return true;
    }

    // Software-visible clear (models the function clearing a serviced bit).
    bool clear_pending(unsigned idx, std::string* err = nullptr) {
        if (!check_index(idx, err)) return false;
        if (!set_pba_bit(idx, false, err)) return false;
        ++pba_cleared;
        return true;
    }

    // Deliver a vector that became pending while masked; the PBA bit is
    // cleared (the message would be sent at this point in hardware).
    bool deliver_pending(unsigned idx, std::uint64_t* addr,
                         std::uint32_t* data, std::string* err = nullptr) {
        if (!check_index(idx, err)) return false;
        if (!enable) {
            if (err) *err = "MSI-X disabled";
            return false;
        }
        if (fn_mask) {
            if (err) *err = "MSI-X function mask set";
            return false;
        }
        Entry e;
        if (!read_entry(idx, &e, err)) return false;
        if (e.masked()) {
            if (err) *err = "MSI-X vector still masked";
            return false;
        }
        if (!pending(idx, err)) {
            if (err) *err = "MSI-X no pending interrupt";
            return false;
        }
        if (!set_pba_bit(idx, false, err)) return false;
        if (addr) *addr = e.addr;
        if (data) *data = e.data;
        ++triggers_flushed;
        return true;
    }

private:
    bool check_index(unsigned idx, std::string* err) {
        if (!root) {
            if (err) *err = "MSI-X: table not attached";
            ++errors;
            return false;
        }
        if (idx >= nvec) {
            if (err) *err = "MSI-X: vector index out of range";
            ++errors;
            return false;
        }
        return true;
    }

    bool set_pba_bit(unsigned idx, bool set, std::string* err) {
        const std::uint64_t off =
            pba_base + (std::uint64_t)(idx / 64u) * 8u;
        const unsigned bit = idx % 64u;
        std::uint64_t v = 0;
        if (!root->mem_read(off, &v, sizeof(v), err)) {
            ++errors;
            return false;
        }
        if (set)
            v |= (std::uint64_t)1u << bit;
        else
            v &= ~((std::uint64_t)1u << bit);
        if (!root->mem_write(off, &v, sizeof(v), err)) {
            ++errors;
            return false;
        }
        if (set) ++pba_set;
        return true;
    }
};

} // namespace cxl_tlm
