// -----------------------------------------------------------------------------
// cxl_dcd_media.h -- Reference DCD media backend (model profile).
//
//   CxlDcdState owns capacity bookkeeping and exposes media_allocate/
//   media_release owner hooks.  This class is a small reference backend that
//   demonstrates the owner side:
//     * allocated DPA extents (64-byte granularity), overlap checked;
//     * sparse 64-byte block storage inside allocated extents;
//     * read/write/zero/sanitize with explicit out-of-extent errors;
//     * injected bad blocks and a scan that reports them.
//
//   It is a reference model, not a physical media implementation.  A platform
//   can replace it by installing its own hooks on CxlDcdState.
// -----------------------------------------------------------------------------
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace cxl_tlm {

struct CxlDcdMediaRange {
    std::uint64_t dpa = 0;
    std::uint64_t len = 0;
};

struct CxlDcdReferenceMedia {
    static constexpr std::uint64_t kBlock = 64;

    std::map<std::uint64_t, std::uint64_t> extents;   // dpa -> length
    std::map<std::uint64_t, std::array<std::uint8_t, kBlock>> blocks;
    std::set<std::uint64_t> bad_blocks;               // block base DPA

    std::uint64_t alloc_ok = 0, alloc_fail = 0;
    std::uint64_t release_ok = 0, release_fail = 0;
    std::uint64_t writes = 0, zeros = 0, sanitizes = 0;
    mutable std::uint64_t reads = 0, bad_access = 0;
    std::uint64_t bad_inject = 0;

    void reset()
    {
        extents.clear();
        blocks.clear();
        bad_blocks.clear();
        alloc_ok = alloc_fail = release_ok = release_fail = 0;
        reads = writes = zeros = sanitizes = bad_access = bad_inject = 0;
    }

    std::uint64_t allocated_bytes() const
    {
        std::uint64_t n = 0;
        for (const auto& e : extents) n += e.second;
        return n;
    }

    // True when [dpa, dpa+len) is 64-byte aligned and fully inside one
    // allocated extent.
    bool contains(std::uint64_t dpa, std::uint64_t len) const
    {
        if (len == 0 || (dpa % kBlock) != 0 || (len % kBlock) != 0) return false;
        auto it = extents.upper_bound(dpa);
        if (it == extents.begin()) return false;
        --it;
        return dpa >= it->first && (dpa - it->first) <= it->second &&
               len <= it->second - (dpa - it->first);
    }

    bool allocate(std::uint64_t dpa, std::uint64_t len)
    {
        if (len == 0 || (dpa % kBlock) != 0 || (len % kBlock) != 0) {
            ++alloc_fail;
            return false;
        }
        auto it = extents.lower_bound(dpa);
        if (it != extents.end() && dpa + len > it->first) {
            ++alloc_fail;
            return false;
        }
        if (it != extents.begin()) {
            auto prev = std::prev(it);
            if (prev->first + prev->second > dpa) {
                ++alloc_fail;
                return false;
            }
        }
        extents[dpa] = len;
        ++alloc_ok;
        return true;
    }

    bool release(std::uint64_t dpa, std::uint64_t len)
    {
        if (!contains(dpa, len)) {
            ++release_fail;
            return false;
        }
        auto it = extents.upper_bound(dpa);
        --it;
        const std::uint64_t base = it->first;
        const std::uint64_t total = it->second;
        extents.erase(it);
        if (dpa > base) extents[base] = dpa - base;
        const std::uint64_t right = base + total;
        const std::uint64_t rend = dpa + len;
        if (rend < right) extents[rend] = right - rend;
        for (auto b = blocks.lower_bound(dpa); b != blocks.end() &&
             b->first < rend; ) {
            b = blocks.erase(b);
        }
        for (auto b = bad_blocks.lower_bound(dpa); b != bad_blocks.end() &&
             *b < rend; ) {
            b = bad_blocks.erase(b);
        }
        ++release_ok;
        return true;
    }

    bool write(std::uint64_t dpa, const void* data, std::size_t n)
    {
        if (!contains(dpa, n)) {
            ++bad_access;
            return false;
        }
        const auto* p = static_cast<const std::uint8_t*>(data);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t a = dpa + i;
            blocks[a & ~(kBlock - 1)][a % kBlock] = p[i];
        }
        ++writes;
        return true;
    }

    bool read(std::uint64_t dpa, void* data, std::size_t n) const
    {
        if (!contains(dpa, n)) {
            ++bad_access;
            return false;
        }
        auto* p = static_cast<std::uint8_t*>(data);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t a = dpa + i;
            auto it = blocks.find(a & ~(kBlock - 1));
            p[i] = (it == blocks.end()) ? 0 : it->second[a % kBlock];
        }
        ++reads;
        return true;
    }

    bool zero(std::uint64_t dpa, std::uint64_t len)
    {
        if (!contains(dpa, len)) {
            ++bad_access;
            return false;
        }
        for (std::uint64_t a = dpa; a < dpa + len; a += kBlock)
            blocks.erase(a);
        ++zeros;
        return true;
    }

    bool sanitize(std::uint64_t dpa, std::uint64_t len)
    {
        if (!zero(dpa, len)) return false;
        for (auto b = bad_blocks.lower_bound(dpa); b != bad_blocks.end() &&
             *b < dpa + len; ) {
            b = bad_blocks.erase(b);
        }
        ++sanitizes;
        return true;
    }

    bool inject_bad(std::uint64_t dpa)
    {
        if (!contains(dpa & ~(kBlock - 1), kBlock)) {
            ++bad_inject;
            return false;
        }
        bad_blocks.insert(dpa & ~(kBlock - 1));
        return true;
    }

    bool scan(std::uint64_t dpa, std::uint64_t len,
              std::vector<CxlDcdMediaRange>* bad) const
    {
        if (!contains(dpa, len)) return false;
        if (bad) bad->clear();
        for (auto it = bad_blocks.lower_bound(dpa); it != bad_blocks.end() &&
             *it < dpa + len; ++it) {
            if (bad) bad->push_back(CxlDcdMediaRange{*it, kBlock});
        }
        return true;
    }
};

} // namespace cxl_tlm
