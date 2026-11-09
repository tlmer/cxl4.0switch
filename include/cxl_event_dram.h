// -----------------------------------------------------------------------------
// cxl_event_dram.h -- DRAM Event Record (CXL 3.1/3.2).
//
//   CXL 3.1 section 8.2.9.2.1.2, Table 8-46; Linux 6.18
//   include/cxl/event.h (struct cxl_event_dram / cxl_event_media_hdr) and
//   drivers/cxl/cxlmem.h:622 (CXL_EVENT_DRAM_UUID).
//
//   Raw record (0x80 bytes):
//     0x00 uuid[16] = 601dcbb3-9c06-4eab-b8af-4e9bfb5c9624
//     0x10 common event header (0x20)
//     0x30 payload[0x50]:
//       0x00 phys_addr(8), 0x08 descriptor, 0x09 type,
//       0x0A transaction_type, 0x0B validity_flags(2),
//       0x0D channel, 0x0E rank,
//       0x0F nibble_mask[3], 0x12 bank_group, 0x13 bank,
//       0x14 row[3], 0x17 column(2), 0x19 correction_mask[32],
//       0x39 component_id[16], 0x49 sub_channel,
//       0x4A cme_threshold_ev_flags, 0x4B cvme_count[3],
//       0x4E sub_type, 0x4F reserved
//
//   The generic event log (CxlType3Commands::add_event) and the CPER bridge
//   (cxl_event_cper.h) accept this record unchanged.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_commands.h"
#include "cxl_event_cper.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cxl_tlm {

constexpr std::uint8_t kCxlEventDramUuid[16] = {
    0x60, 0x1d, 0xcb, 0xb3, 0x9c, 0x06, 0x4e, 0xab,
    0xb8, 0xaf, 0x4e, 0x9b, 0xfb, 0x5c, 0x96, 0x24,
};

struct CxlDramEvent {
    // Common event-record header.
    std::uint8_t severity = 0;
    std::uint16_t handle = 0;
    std::uint16_t related = 0;
    std::uint64_t timestamp = 0;
    std::uint8_t maint_op_class = 0;
    std::uint8_t maint_op_sub_class = 0;
    std::uint16_t ld_id = 0;
    std::uint8_t head_id = 0;

    // media_hdr tail + DRAM payload.
    std::uint64_t phys_addr = 0;
    std::uint8_t descriptor = 0;
    std::uint8_t type = 0;
    std::uint8_t transaction_type = 0;
    std::uint16_t validity_flags = 0;
    std::uint8_t channel = 0;
    std::uint8_t rank = 0;
    std::array<std::uint8_t, 3> nibble_mask{};
    std::uint8_t bank_group = 0;
    std::uint8_t bank = 0;
    std::array<std::uint8_t, 3> row{};
    std::uint16_t column = 0;
    std::array<std::uint8_t, 32> correction_mask{};
    std::array<std::uint8_t, 16> component_id{};
    std::uint8_t sub_channel = 0;
    std::uint8_t cme_threshold_ev_flags = 0;
    std::array<std::uint8_t, 3> cvme_count{};
    std::uint8_t sub_type = 0;

    static constexpr std::uint8_t kRecordLength = 0x80;

    // CxlEventRecord payload-only view (common-header tail fields are not
    // representable; use build_raw() when they matter).
    void build(CxlEventRecord* r) const
    {
        r->handle = handle;
        r->related = related;
        r->severity = severity;
        r->timestamp = timestamp;
        std::memcpy(r->uuid.data(), kCxlEventDramUuid, 16);
        r->data.fill(0);
        put64(r->data.data(), 0x00, phys_addr);
        r->data[0x08] = descriptor;
        r->data[0x09] = type;
        r->data[0x0A] = transaction_type;
        put16(r->data.data(), 0x0B, validity_flags);
        r->data[0x0D] = channel;
        r->data[0x0E] = rank;
        for (int i = 0; i < 3; ++i) r->data[0x0F + i] = nibble_mask[(std::size_t)i];
        r->data[0x12] = bank_group;
        r->data[0x13] = bank;
        for (int i = 0; i < 3; ++i) r->data[0x14 + i] = row[(std::size_t)i];
        put16(r->data.data(), 0x17, column);
        std::memcpy(r->data.data() + 0x19, correction_mask.data(), 32);
        std::memcpy(r->data.data() + 0x39, component_id.data(), 16);
        r->data[0x49] = sub_channel;
        r->data[0x4A] = cme_threshold_ev_flags;
        for (int i = 0; i < 3; ++i) r->data[0x4B + i] = cvme_count[(std::size_t)i];
        r->data[0x4E] = sub_type;
    }

    std::vector<std::uint8_t> build_raw() const
    {
        std::vector<std::uint8_t> v(kRecordLength, 0);
        std::memcpy(v.data(), kCxlEventDramUuid, 16);
        v[0x10] = kRecordLength;
        v[0x11] = (std::uint8_t)(severity & 0x3);
        put16(v, 0x14, handle);
        put16(v, 0x16, related);
        put64(v, 0x18, timestamp);
        v[0x20] = maint_op_class;
        v[0x21] = maint_op_sub_class;
        put16(v, 0x22, ld_id);
        v[0x24] = head_id;
        const std::size_t p = 0x30;
        put64(v, p + 0x00, phys_addr);
        v[p + 0x08] = descriptor;
        v[p + 0x09] = type;
        v[p + 0x0A] = transaction_type;
        put16(v, p + 0x0B, validity_flags);
        v[p + 0x0D] = channel;
        v[p + 0x0E] = rank;
        for (int i = 0; i < 3; ++i) v[p + 0x0F + (std::size_t)i] = nibble_mask[(std::size_t)i];
        v[p + 0x12] = bank_group;
        v[p + 0x13] = bank;
        for (int i = 0; i < 3; ++i) v[p + 0x14 + (std::size_t)i] = row[(std::size_t)i];
        put16(v, p + 0x17, column);
        std::memcpy(v.data() + p + 0x19, correction_mask.data(), 32);
        std::memcpy(v.data() + p + 0x39, component_id.data(), 16);
        v[p + 0x49] = sub_channel;
        v[p + 0x4A] = cme_threshold_ev_flags;
        for (int i = 0; i < 3; ++i) v[p + 0x4B + (std::size_t)i] = cvme_count[(std::size_t)i];
        v[p + 0x4E] = sub_type;
        return v;
    }

    static bool parse(const std::uint8_t* raw, std::size_t n,
                      CxlDramEvent* out)
    {
        if (!raw || !out || n < kRecordLength) return false;
        if (std::memcmp(raw, kCxlEventDramUuid, 16) != 0) return false;
        if (raw[0x10] != kRecordLength) return false;
        out->severity = (std::uint8_t)(raw[0x11] & 0x3);
        out->handle = get16(raw, 0x14);
        out->related = get16(raw, 0x16);
        out->timestamp = get64(raw, 0x18);
        out->maint_op_class = raw[0x20];
        out->maint_op_sub_class = raw[0x21];
        out->ld_id = get16(raw, 0x22);
        out->head_id = raw[0x24];
        const std::size_t p = 0x30;
        out->phys_addr = get64(raw, p + 0x00);
        out->descriptor = raw[p + 0x08];
        out->type = raw[p + 0x09];
        out->transaction_type = raw[p + 0x0A];
        out->validity_flags = get16(raw, p + 0x0B);
        out->channel = raw[p + 0x0D];
        out->rank = raw[p + 0x0E];
        for (int i = 0; i < 3; ++i) out->nibble_mask[(std::size_t)i] = raw[p + 0x0F + (std::size_t)i];
        out->bank_group = raw[p + 0x12];
        out->bank = raw[p + 0x13];
        for (int i = 0; i < 3; ++i) out->row[(std::size_t)i] = raw[p + 0x14 + (std::size_t)i];
        out->column = get16(raw, p + 0x17);
        std::memcpy(out->correction_mask.data(), raw + p + 0x19, 32);
        std::memcpy(out->component_id.data(), raw + p + 0x39, 16);
        out->sub_channel = raw[p + 0x49];
        out->cme_threshold_ev_flags = raw[p + 0x4A];
        for (int i = 0; i < 3; ++i) out->cvme_count[(std::size_t)i] = raw[p + 0x4B + (std::size_t)i];
        out->sub_type = raw[p + 0x4E];
        return true;
    }

private:
    static std::uint16_t get16(const std::uint8_t* p, std::size_t o)
    {
        return (std::uint16_t)(p[o] | (p[o + 1] << 8));
    }
    static std::uint64_t get64(const std::uint8_t* p, std::size_t o)
    {
        std::uint64_t x = 0;
        for (int i = 0; i < 8; ++i) x |= (std::uint64_t)p[o + i] << (8 * i);
        return x;
    }
    static void put16(std::uint8_t* p, std::size_t o, std::uint16_t x)
    {
        p[o] = (std::uint8_t)(x & 0xFF);
        p[o + 1] = (std::uint8_t)((x >> 8) & 0xFF);
    }
    static void put16(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint16_t x)
    {
        v[o] = (std::uint8_t)(x & 0xFF);
        v[o + 1] = (std::uint8_t)((x >> 8) & 0xFF);
    }
    static void put64(std::uint8_t* p, std::size_t o, std::uint64_t x)
    {
        for (int i = 0; i < 8; ++i)
            p[o + i] = (std::uint8_t)((x >> (8 * i)) & 0xFF);
    }
    static void put64(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint64_t x)
    {
        for (int i = 0; i < 8; ++i)
            v[o + i] = (std::uint8_t)((x >> (8 * i)) & 0xFF);
    }
};

} // namespace cxl_tlm
