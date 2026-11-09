// -----------------------------------------------------------------------------
// cxl_event_gen_media.h -- General Media Event Record (CXL 3.1/3.2).
//
//   CXL 3.1 section 8.2.9.2.1.1, Table 8-45; Linux 6.18
//   include/cxl/event.h (struct cxl_event_gen_media / cxl_event_media_hdr)
//   and drivers/cxl/cxlmem.h:614 (CXL_EVENT_GEN_MEDIA_UUID).
//
//   Raw record (0x80 bytes):
//     0x00 uuid[16] = fbcd0a77-c260-417f-85a9-088b1621eba6
//     0x10 common event header (0x20): length=0x80, flags[3] (severity
//          [1:0]), handle, related, timestamp, maint_op_class,
//          maint_op_sub_class, ld_id, head_id, reserved[11]
//     0x30 payload[0x50] (media_hdr tail + gen-media fields):
//       0x00 phys_addr(8), 0x08 descriptor, 0x09 type,
//       0x0A transaction_type, 0x0B validity_flags(2),
//       0x0D channel, 0x0E rank, 0x0F device[3],
//       0x12 component_id[16], 0x22 cme_threshold_ev_flags,
//       0x23 cme_count[3], 0x26 sub_type, 0x27 reserved[41]
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

constexpr std::uint8_t kCxlEventGenMediaUuid[16] = {
    0xfb, 0xcd, 0x0a, 0x77, 0xc2, 0x60, 0x41, 0x7f,
    0x85, 0xa9, 0x08, 0x8b, 0x16, 0x21, 0xeb, 0xa6,
};

struct CxlGenMediaEvent {
    // Common event-record header.
    std::uint8_t severity = 0;
    std::uint16_t handle = 0;
    std::uint16_t related = 0;
    std::uint64_t timestamp = 0;
    std::uint8_t maint_op_class = 0;
    std::uint8_t maint_op_sub_class = 0;
    std::uint16_t ld_id = 0;
    std::uint8_t head_id = 0;

    // media_hdr tail + gen-media payload.
    std::uint64_t phys_addr = 0;
    std::uint8_t descriptor = 0;
    std::uint8_t type = 0;
    std::uint8_t transaction_type = 0;
    std::uint16_t validity_flags = 0;
    std::uint8_t channel = 0;
    std::uint8_t rank = 0;
    std::array<std::uint8_t, 3> device{};
    std::array<std::uint8_t, 16> component_id{};
    std::uint8_t cme_threshold_ev_flags = 0;
    std::array<std::uint8_t, 3> cme_count{};
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
        std::memcpy(r->uuid.data(), kCxlEventGenMediaUuid, 16);
        r->data.fill(0);
        put64(r->data.data(), 0x00, phys_addr);
        r->data[0x08] = descriptor;
        r->data[0x09] = type;
        r->data[0x0A] = transaction_type;
        put16(r->data.data(), 0x0B, validity_flags);
        r->data[0x0D] = channel;
        r->data[0x0E] = rank;
        r->data[0x0F] = device[0];
        r->data[0x10] = device[1];
        r->data[0x11] = device[2];
        std::memcpy(r->data.data() + 0x12, component_id.data(), 16);
        r->data[0x22] = cme_threshold_ev_flags;
        r->data[0x23] = cme_count[0];
        r->data[0x24] = cme_count[1];
        r->data[0x25] = cme_count[2];
        r->data[0x26] = sub_type;
    }

    std::vector<std::uint8_t> build_raw() const
    {
        std::vector<std::uint8_t> v(kRecordLength, 0);
        std::memcpy(v.data(), kCxlEventGenMediaUuid, 16);
        v[0x10] = kRecordLength;
        v[0x11] = (std::uint8_t)(severity & 0x3);
        put16(v, 0x14, handle);
        put16(v, 0x16, related);
        put64(v, 0x18, timestamp);
        v[0x20] = maint_op_class;
        v[0x21] = maint_op_sub_class;
        put16(v, 0x22, ld_id);
        v[0x24] = head_id;
        put64(v, 0x30 + 0x00, phys_addr);
        v[0x30 + 0x08] = descriptor;
        v[0x30 + 0x09] = type;
        v[0x30 + 0x0A] = transaction_type;
        put16(v, 0x30 + 0x0B, validity_flags);
        v[0x30 + 0x0D] = channel;
        v[0x30 + 0x0E] = rank;
        v[0x30 + 0x0F] = device[0];
        v[0x30 + 0x10] = device[1];
        v[0x30 + 0x11] = device[2];
        std::memcpy(v.data() + 0x30 + 0x12, component_id.data(), 16);
        v[0x30 + 0x22] = cme_threshold_ev_flags;
        v[0x30 + 0x23] = cme_count[0];
        v[0x30 + 0x24] = cme_count[1];
        v[0x30 + 0x25] = cme_count[2];
        v[0x30 + 0x26] = sub_type;
        return v;
    }

    static bool parse(const std::uint8_t* raw, std::size_t n,
                      CxlGenMediaEvent* out)
    {
        if (!raw || !out || n < kRecordLength) return false;
        if (std::memcmp(raw, kCxlEventGenMediaUuid, 16) != 0) return false;
        if (raw[0x10] != kRecordLength) return false;
        out->severity = (std::uint8_t)(raw[0x11] & 0x3);
        out->handle = get16(raw, 0x14);
        out->related = get16(raw, 0x16);
        out->timestamp = get64(raw, 0x18);
        out->maint_op_class = raw[0x20];
        out->maint_op_sub_class = raw[0x21];
        out->ld_id = get16(raw, 0x22);
        out->head_id = raw[0x24];
        out->phys_addr = get64(raw, 0x30 + 0x00);
        out->descriptor = raw[0x30 + 0x08];
        out->type = raw[0x30 + 0x09];
        out->transaction_type = raw[0x30 + 0x0A];
        out->validity_flags = get16(raw, 0x30 + 0x0B);
        out->channel = raw[0x30 + 0x0D];
        out->rank = raw[0x30 + 0x0E];
        for (int i = 0; i < 3; ++i) out->device[i] = raw[0x30 + 0x0F + i];
        std::memcpy(out->component_id.data(), raw + 0x30 + 0x12, 16);
        out->cme_threshold_ev_flags = raw[0x30 + 0x22];
        for (int i = 0; i < 3; ++i) out->cme_count[i] = raw[0x30 + 0x23 + i];
        out->sub_type = raw[0x30 + 0x26];
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
