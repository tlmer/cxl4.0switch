// -----------------------------------------------------------------------------
// cxl_event_dcd.h -- Dynamic Capacity Event Record (CXL 4.0 Table 8-229).
//
//   Raw record (0x80 bytes):
//     0x00 uuid[16] = ca95afa7-f183-4018-8c2f-95268e101a2a
//     0x10 length (0x80), 0x11 flags (severity[1:0] = 0 informational)
//     0x14 handle, 0x16 related, 0x18 timestamp
//     0x30 event_type          0=Add, 1=Release, 2=Forced Release,
//                              3=Region Configuration Updated
//     0x31 validity_flags
//     0x32 host_id (u16)
//     0x34 region_index        valid only for Region Configuration Updated
//     0x35 flags               bit0 More (multi-extent grouping)
//     0x36 reserved[2]
//     0x38 extent (0x28): start_dpa(8), length(8), tag[16], shared_seq(2),
//                         reserved[6]
//     0x60 reserved[0x18]
//     0x78 num_avail_extents (u32)
//     0x7C num_avail_tags (u32)
//
//   Event records in class 4 are always Informational.  A More=1 record is
//   followed by another record from the same device action; the host treats
//   the group as one offer/release.
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cxl_tlm {

constexpr std::uint8_t kCxlEventDcdUuid[16] = {
    0xca, 0x95, 0xaf, 0xa7, 0xf1, 0x83, 0x40, 0x18,
    0x8c, 0x2f, 0x95, 0x26, 0x8e, 0x10, 0x1a, 0x2a,
};

struct CxlDcdEvent {
    static constexpr std::uint8_t kRecordLength = 0x80;

    // Common event-record header.
    std::uint8_t severity = 0;
    std::uint16_t handle = 0;
    std::uint16_t related = 0;
    std::uint64_t timestamp = 0;

    // Table 8-229 payload.
    std::uint8_t event_type = 0;
    std::uint8_t validity_flags = 0;
    std::uint16_t host_id = 0;
    std::uint8_t region_index = 0;
    std::uint8_t flags = 0;
    std::uint64_t start_dpa = 0;
    std::uint64_t length = 0;
    std::array<std::uint8_t, 16> tag{};
    std::uint16_t shared_extent_seq = 0;
    std::uint32_t num_avail_extents = 0;
    std::uint32_t num_avail_tags = 0;

    bool more() const { return (flags & 0x01) != 0; }

    static bool parse(const std::uint8_t* raw, std::size_t n,
                      CxlDcdEvent* out)
    {
        if (!raw || !out || n < kRecordLength) return false;
        if (std::memcmp(raw, kCxlEventDcdUuid, 16) != 0) return false;
        if (raw[0x10] != kRecordLength) return false;
        *out = CxlDcdEvent();
        out->severity = (std::uint8_t)(raw[0x11] & 0x3);
        out->handle = (std::uint16_t)(raw[0x14] | (raw[0x15] << 8));
        out->related = (std::uint16_t)(raw[0x16] | (raw[0x17] << 8));
        out->timestamp = rd64(raw, 0x18);
        out->event_type = raw[0x30];
        out->validity_flags = raw[0x31];
        out->host_id = (std::uint16_t)(raw[0x32] | (raw[0x33] << 8));
        out->region_index = raw[0x34];
        out->flags = raw[0x35];
        out->start_dpa = rd64(raw, 0x38);
        out->length = rd64(raw, 0x40);
        std::memcpy(out->tag.data(), raw + 0x48, 16);
        out->shared_extent_seq =
            (std::uint16_t)(raw[0x58] | (raw[0x59] << 8));
        out->num_avail_extents = rd32(raw, 0x78);
        out->num_avail_tags = rd32(raw, 0x7C);
        return true;
    }

private:
    static std::uint32_t rd32(const std::uint8_t* p, std::size_t o)
    {
        return (std::uint32_t)p[o] | ((std::uint32_t)p[o + 1] << 8) |
               ((std::uint32_t)p[o + 2] << 16) |
               ((std::uint32_t)p[o + 3] << 24);
    }
    static std::uint64_t rd64(const std::uint8_t* p, std::size_t o)
    {
        std::uint64_t x = 0;
        for (int i = 0; i < 8; ++i)
            x |= (std::uint64_t)p[o + (std::size_t)i] << (8 * i);
        return x;
    }
};

} // namespace cxl_tlm
