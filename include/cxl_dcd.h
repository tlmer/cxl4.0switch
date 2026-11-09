// -----------------------------------------------------------------------------
// cxl_dcd.h -- CXL 4.0 Dynamic Capacity Device (DCD) model profile.
//
//   Implements the mailbox-side state for:
//     0x4800 Get Dynamic Capacity Configuration   (Table 8-345/8-346/8-347)
//     0x4801 Get Dynamic Capacity Extent List     (Table 8-348/8-349, 8-230)
//     0x4802 Add Dynamic Capacity Response        (Table 8-350/8-351)
//     0x4803 Release Dynamic Capacity             (Table 8-352/8-351)
//
//   This is an optional profile: the caller enables it only for a DCD-capable
//   device.  The model keeps two extent lists:
//     offered  -- capacity the device has made available to the host;
//     assigned -- capacity the host has acknowledged with Add Response.
//   Add moves validated extents from offered to assigned; Release moves them
//   back.  More-flag grouping is staged per command until the final payload.
//
//   The model does not own a media array; `media_allocate` / `media_release`
//   are optional owner hooks.  A final Add acquires every extent before the
//   capacity moves (partial failure rolls back and returns Resources
//   Exhausted); Release frees committed extents.  Extents are validated
//   against the configured regions (block alignment, non-overlap, region
//   boundary).  Tags are opaque and preserved; the DCD profile shares tag
//   capacity across extents (a distinct tag counts once while any assigned
//   extent holds it, CXL 3.2 Table 8-347), and Add returns Resources
//   Exhausted when no new tag is available.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_mailbox.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

namespace cxl_tlm {

struct CxlDcdRegion {
    std::uint64_t base = 0;          // 256 MB aligned in a real device
    std::uint64_t decode_len = 0;
    std::uint64_t region_len = 0;    // 0 => region unavailable
    std::uint64_t block_size = 0;    // power of two, multiple of 64
    std::uint32_t dsmad_handle = 0;
    std::uint8_t flags = 0;          // bit0 sanitize-on-release
};

struct CxlDcdExtent {
    std::uint64_t dpa = 0;
    std::uint64_t len = 0;
    std::array<std::uint8_t, 16> tag{};
    std::uint16_t seq = 0;
};

struct CxlDcdState {
    std::vector<CxlDcdRegion> regions;
    std::vector<CxlDcdExtent> offered;
    std::vector<CxlDcdExtent> assigned;
    std::vector<CxlDcdExtent> pending_add;
    std::vector<CxlDcdExtent> pending_release;

    std::uint32_t total_extents = 256;
    // DCD tag accounting (CXL 3.2 Table 8-347: supported tags / available
    // tags).  Tags are opaque 16-byte values supplied on the extent.  The
    // model profile shares tag capacity: a distinct tag counts once while any
    // assigned extent holds it, so several extents may carry the same tag.
    // Zero tags are "no tag" and are not accounted.
    std::uint32_t total_tags = 16;   // model profile
    std::uint32_t generation = 1;

    std::uint64_t add_ok = 0, release_ok = 0, invalid = 0;
    std::uint64_t invalid_extent = 0, resources = 0, bad_payload = 0;

    // Optional media-owner hooks.  The capacity model does not own backing
    // storage; a platform that does attaches these callbacks.  Add Response
    // acquires every extent before committing (a failure rolls back and
    // returns Resources Exhausted); Release frees the committed extents.
    // Set both callbacks together so a partial acquisition can be unwound.
    std::function<bool(const CxlDcdExtent&)> media_allocate;
    std::function<void(const CxlDcdExtent&)> media_release;
    std::uint64_t media_alloc_ok = 0;    // extents acquired
    std::uint64_t media_alloc_fail = 0;  // Add commands rolled back
    std::uint64_t media_release_ok = 0;  // extents released

    // ---- 0x4800 Get Dynamic Capacity Configuration -----------------------
    bool get_config(const std::vector<std::uint8_t>& in,
                    std::vector<std::uint8_t>* out, std::uint16_t* rc)
    {
        if (in.size() != 2) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        const std::uint8_t want = in[0];
        const std::uint8_t start = in[1];
        if ((std::size_t)start > regions.size()) {
            ++invalid;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        std::size_t n = regions.size() - start;
        if (n > want) n = want;
        const std::size_t base = 8;
        out->assign(base + n * 0x28 + 16, 0);
        (*out)[0] = (std::uint8_t)regions.size();
        (*out)[1] = (std::uint8_t)n;
        for (std::size_t i = 0; i < n; ++i) {
            const CxlDcdRegion& r = regions[start + i];
            const std::size_t o = base + i * 0x28;
            put64(*out, o + 0x00, r.base);
            put64(*out, o + 0x08, r.decode_len);
            put64(*out, o + 0x10, r.region_len);
            put64(*out, o + 0x18, r.block_size);
            put32(*out, o + 0x20, r.dsmad_handle);
            (*out)[o + 0x24] = r.flags;
        }
        const std::size_t tail = base + n * 0x28;
        put32(*out, tail + 0x00, total_extents);
        put32(*out, tail + 0x04, available_extents());
        put32(*out, tail + 0x08, total_tags);
        put32(*out, tail + 0x0C, available_tags());
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    // ---- 0x4801 Get Dynamic Capacity Extent List -------------------------
    bool get_extent_list(const std::vector<std::uint8_t>& in,
                         std::vector<std::uint8_t>* out, std::uint16_t* rc)
    {
        if (in.size() != 8) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        const std::uint32_t want = get32(in, 0);
        const std::uint32_t start = get32(in, 4);
        const std::size_t total = assigned.size();
        if ((std::size_t)start > total) {
            ++invalid;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        std::size_t n = total - start;
        if (n > want) n = want;
        out->assign(0x10 + n * 0x28, 0);
        put32(*out, 0x00, (std::uint32_t)n);
        put32(*out, 0x04, (std::uint32_t)total);
        put32(*out, 0x08, generation);
        for (std::size_t i = 0; i < n; ++i) {
            const CxlDcdExtent& e = assigned[start + i];
            const std::size_t o = 0x10 + i * 0x28;
            put64(*out, o + 0x00, e.dpa);
            put64(*out, o + 0x08, e.len);
            std::copy(e.tag.begin(), e.tag.end(), out->begin() + o + 0x10);
            put16(*out, o + 0x20, e.seq);
        }
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    // ---- 0x4802 Add Dynamic Capacity Response ----------------------------
    // `applied`, when non-null, receives the extents committed by the final
    // (More=0) payload.  It stays empty for a staged More=1 payload and for
    // any rejected request, so the caller emits Add Capacity events only for
    // capacity that actually moved to the assigned list.
    bool add_response(const std::vector<std::uint8_t>& in, std::uint16_t* rc,
                      std::vector<CxlDcdExtent>* applied = nullptr)
    {
        if (applied) applied->clear();
        std::vector<CxlDcdExtent> cur;
        if (!parse_request(in, &cur, rc)) {
            pending_add.clear();
            return true;
        }
        if (in[4] & 0x01) {                       // More: stage this payload
            pending_add.insert(pending_add.end(), cur.begin(), cur.end());
            ++add_ok;
            *rc = MBX_RC_SUCCESS;
            return true;
        }
        std::vector<CxlDcdExtent> all = pending_add;
        pending_add.clear();
        all.insert(all.end(), cur.begin(), cur.end());
        if (!validate_ranges(all)) {
            invalid_extent++;
            *rc = MBX_RC_INVALID_EXTENT_LIST;
            return true;
        }
        // Tag accounting: a tag may be shared by several extents (DCD shared
        // capacity); each distinct non-zero tag consumes one entry.
        std::vector<std::array<std::uint8_t, 16>> new_tag_set;
        for (const auto& e : all)
            if (!is_zero_tag(e.tag) &&
                std::find(new_tag_set.begin(), new_tag_set.end(), e.tag) ==
                    new_tag_set.end())
                new_tag_set.push_back(e.tag);
        std::uint32_t add_tags = 0;
        for (const auto& t : new_tag_set) {
            bool already = false;
            for (const auto& a : assigned)
                if (!is_zero_tag(a.tag) && a.tag == t) { already = true; break; }
            if (!already) ++add_tags;
        }
        if (used_tags() + add_tags > total_tags) {
            ++resources;
            *rc = MBX_RC_RESOURCES_EXHAUSTED;
            return true;
        }
        // Work on copies so a failed add leaves the device state unchanged.
        std::vector<CxlDcdExtent> off = offered;
        std::vector<CxlDcdExtent> asg = assigned;
        for (const auto& e : all) {
            if (overlap_any(asg, e) || !cut_from(&off, e)) {
                invalid_extent++;
                *rc = MBX_RC_INVALID_EXTENT_LIST;
                return true;
            }
        }
        if (off.size() + asg.size() + all.size() > total_extents) {
            ++resources;
            *rc = MBX_RC_RESOURCES_EXHAUSTED;
            return true;
        }
        // Acquire backing media before moving capacity.  A partial failure
        // unwinds what was acquired and leaves offered/assigned untouched.
        if (media_allocate) {
            std::size_t acquired = 0;
            bool ok = true;
            for (const auto& e : all) {
                if (!media_allocate(e)) { ok = false; break; }
                ++acquired;
            }
            if (!ok) {
                if (media_release)
                    for (std::size_t i = acquired; i-- > 0;)
                        media_release(all[i]);
                ++media_alloc_fail;
                ++resources;
                *rc = MBX_RC_RESOURCES_EXHAUSTED;
                return true;
            }
            media_alloc_ok += all.size();
        }
        asg.insert(asg.end(), all.begin(), all.end());
        offered.swap(off);
        assigned.swap(asg);
        ++generation;
        ++add_ok;
        if (applied) *applied = all;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    // ---- 0x4803 Release Dynamic Capacity ---------------------------------
    bool release(const std::vector<std::uint8_t>& in, std::uint16_t* rc,
                 std::vector<CxlDcdExtent>* applied = nullptr)
    {
        if (applied) applied->clear();
        std::vector<CxlDcdExtent> cur;
        if (!parse_request(in, &cur, rc)) {
            pending_release.clear();
            return true;
        }
        if (in[4] & 0x01) {                       // More: stage this payload
            pending_release.insert(pending_release.end(), cur.begin(), cur.end());
            ++release_ok;
            *rc = MBX_RC_SUCCESS;
            return true;
        }
        std::vector<CxlDcdExtent> all = pending_release;
        pending_release.clear();
        all.insert(all.end(), cur.begin(), cur.end());
        if (all.empty()) {
            ++invalid;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        if (!validate_ranges(all)) {
            invalid_extent++;
            *rc = MBX_RC_INVALID_EXTENT_LIST;
            return true;
        }
        std::vector<CxlDcdExtent> off = offered;
        std::vector<CxlDcdExtent> asg = assigned;
        for (const auto& e : all) {
            if (!cut_from(&asg, e)) {
                invalid_extent++;
                *rc = MBX_RC_INVALID_EXTENT_LIST;
                return true;
            }
        }
        if (off.size() + asg.size() + all.size() > total_extents) {
            ++resources;
            *rc = MBX_RC_RESOURCES_EXHAUSTED;
            return true;
        }
        off.insert(off.end(), all.begin(), all.end());
        offered.swap(off);
        assigned.swap(asg);
        if (media_release) {
            for (const auto& e : all) media_release(e);
            media_release_ok += all.size();
        }
        ++generation;
        ++release_ok;
        if (applied) *applied = all;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    std::uint32_t available_extents() const
    {
        const std::size_t used = offered.size() + assigned.size();
        return (used >= total_extents) ? 0 : (std::uint32_t)(total_extents - used);
    }

    static bool is_zero_tag(const std::array<std::uint8_t, 16>& t)
    {
        for (std::uint8_t b : t)
            if (b != 0) return false;
        return true;
    }

    // Distinct non-zero tags currently held by assigned extents.  Several
    // extents may share one tag (DCD shared capacity); the tag counts once.
    std::uint32_t used_tags() const
    {
        std::vector<std::array<std::uint8_t, 16>> tags;
        for (const auto& e : assigned) {
            if (is_zero_tag(e.tag)) continue;
            if (std::find(tags.begin(), tags.end(), e.tag) == tags.end())
                tags.push_back(e.tag);
        }
        return (std::uint32_t)tags.size();
    }

    std::uint32_t available_tags() const
    {
        return used_tags() >= total_tags ? 0 : total_tags - used_tags();
    }

private:
    static void put16(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint16_t x)
    {
        v[o] = (std::uint8_t)(x & 0xFF);
        v[o + 1] = (std::uint8_t)(x >> 8);
    }
    static void put32(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint32_t x)
    {
        for (int i = 0; i < 4; ++i) v[o + (std::size_t)i] =
            (std::uint8_t)(x >> (8 * i));
    }
    static void put64(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint64_t x)
    {
        for (int i = 0; i < 8; ++i) v[o + (std::size_t)i] =
            (std::uint8_t)(x >> (8 * i));
    }
    static std::uint16_t get16(const std::vector<std::uint8_t>& v,
                               std::size_t o)
    {
        return (std::uint16_t)(v[o] | (v[o + 1] << 8));
    }
    static std::uint32_t get32(const std::vector<std::uint8_t>& v,
                               std::size_t o)
    {
        return (std::uint32_t)v[o] | ((std::uint32_t)v[o + 1] << 8) |
               ((std::uint32_t)v[o + 2] << 16) |
               ((std::uint32_t)v[o + 3] << 24);
    }
    static std::uint64_t get64(const std::vector<std::uint8_t>& v,
                               std::size_t o)
    {
        std::uint64_t x = 0;
        for (int i = 0; i < 8; ++i)
            x |= (std::uint64_t)v[o + (std::size_t)i] << (8 * i);
        return x;
    }

    static bool overlap(const CxlDcdExtent& a, const CxlDcdExtent& b)
    {
        return a.dpa < b.dpa + b.len && b.dpa < a.dpa + a.len;
    }
    static bool overlap_any(const std::vector<CxlDcdExtent>& list,
                            const CxlDcdExtent& e)
    {
        for (const auto& x : list)
            if (overlap(x, e)) return true;
        return false;
    }

    bool region_contains(const CxlDcdExtent& e) const
    {
        for (const auto& r : regions) {
            if (r.region_len == 0 || r.block_size == 0) continue;
            if (e.dpa < r.base) continue;
            if (e.len == 0 || e.len > r.region_len) continue;
            if (e.dpa - r.base > r.region_len - e.len) continue;
            if (((e.dpa - r.base) % r.block_size) != 0) continue;
            if ((e.len % r.block_size) != 0) continue;
            return true;
        }
        return false;
    }

    bool validate_ranges(const std::vector<CxlDcdExtent>& list)
    {
        for (std::size_t i = 0; i < list.size(); ++i) {
            const auto& e = list[i];
            if ((e.dpa & 0x3F) != 0 || e.len == 0 ||
                e.len > UINT64_MAX - e.dpa || !region_contains(e))
                return false;
            for (std::size_t j = 0; j < i; ++j)
                if (overlap(list[j], e)) return false;
        }
        return true;
    }

    // Remove e from list, splitting the containing extent into the remaining
    // left/right pieces.  Returns false when no single extent contains e.
    static bool cut_from(std::vector<CxlDcdExtent>* list,
                         const CxlDcdExtent& e)
    {
        for (std::size_t i = 0; i < list->size(); ++i) {
            CxlDcdExtent base = (*list)[i];
            const std::uint64_t end = base.dpa + base.len;
            if (e.dpa < base.dpa || e.dpa + e.len > end) continue;
            list->erase(list->begin() + (std::ptrdiff_t)i);
            if (e.dpa > base.dpa) {
                CxlDcdExtent left = base;
                left.len = e.dpa - base.dpa;
                list->push_back(left);
            }
            if (e.dpa + e.len < end) {
                CxlDcdExtent right = base;
                right.dpa = e.dpa + e.len;
                right.len = end - right.dpa;
                list->push_back(right);
            }
            return true;
        }
        return false;
    }

    // Parse the 8-byte DCD request header plus 40-byte extents.
    static bool parse_request(const std::vector<std::uint8_t>& in,
                              std::vector<CxlDcdExtent>* list,
                              std::uint16_t* rc)
    {
        if (in.size() < 8) {
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return false;
        }
        const std::uint32_t count = get32(in, 0);
        if ((in[4] & 0xFE) != 0) {
            *rc = MBX_RC_INVALID_INPUT;
            return false;
        }
        if ((std::uint64_t)count * 40u + 8u != in.size()) {
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return false;
        }
        list->clear();
        list->reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::size_t o = 8 + (std::size_t)i * 40;
            CxlDcdExtent e;
            e.dpa = get64(in, o);
            e.len = get64(in, o + 8);
            std::copy(in.begin() + (std::ptrdiff_t)(o + 0x10),
                      in.begin() + (std::ptrdiff_t)(o + 0x20),
                      e.tag.begin());
            e.seq = get16(in, o + 0x20);
            list->push_back(e);
        }
        return true;
    }
};

} // namespace cxl_tlm
