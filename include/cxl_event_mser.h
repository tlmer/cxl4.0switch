// -----------------------------------------------------------------------------
// cxl_event_mser.h -- CXL 3.2 Memory Sparing Event Record (MSER).
//
//   CXL rev 3.2 section 8.2.10.2.1.4, Table 8-60; Linux 6.18
//   include/cxl/event.h (struct cxl_event_mem_sparing) and
//   drivers/cxl/cxlmem.h:638 (CXL_EVENT_MEM_SPARING_UUID).
//
//   Raw event-record layout (0x80 bytes):
//     0x00  uuid[16]   = e71f3a40-2d29-4092-8a39-4d1c966c7c65
//     0x10  length(1)=0x80, flags[3] (severity in bits[1:0])
//     0x14  handle(2), related(2), timestamp(8)
//     0x20  maint_op_class(1), maint_op_sub_class(1), ld_id(2), head_id(1),
//           reserved[11]
//     0x30  event-specific data[0x50]:
//       0x00 rsv1, 0x01 rsv2, 0x02 flags, 0x03 result,
//       0x04 validity_flags(2), 0x06 reserved1[6],
//       0x0C res_avail(2), 0x0E channel, 0x0F rank,
//       0x10 nibble_mask[3], 0x13 bank_group, 0x14 bank,
//       0x15 row[3], 0x18 column(2), 0x1A component_id[16],
//       0x2A sub_channel, 0x2B reserved2[0x25]
//
//   This header owns the record encoding only.  The event log append path
//   (CxlType3Commands::add_event) and CPER conversion (cxl_event_cper.h) are
//   generic and accept this record unchanged.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_commands.h"
#include "cxl_event_cper.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace cxl_tlm {

// CXL_EVENT_MEM_SPARING_UUID, Linux UUID_INIT(0xe71f3a40, 0x2d29, 0x4092,
// 0x8a, 0x39, 0x4d, 0x1c, 0x96, 0x6c, 0x7c, 0x65).
constexpr std::uint8_t kCxlEventMemSparingUuid[16] = {
    0xe7, 0x1f, 0x3a, 0x40, 0x2d, 0x29, 0x40, 0x92,
    0x8a, 0x39, 0x4d, 0x1c, 0x96, 0x6c, 0x7c, 0x65,
};

struct CxlMemSparingEvent {
    // Common event-record header.
    std::uint8_t severity = 0;       // 0=info, 1=warning, 2=failure, 3=fatal
    std::uint16_t handle = 0;
    std::uint16_t related = 0;
    std::uint64_t timestamp = 0;
    std::uint8_t maint_op_class = 0;
    std::uint8_t maint_op_sub_class = 0;
    std::uint16_t ld_id = 0;
    std::uint8_t head_id = 0;

    // MSER-specific payload (raw fields; validity_flags says which are valid).
    std::uint8_t flags = 0;
    std::uint8_t result = 0;
    std::uint16_t validity_flags = 0;
    std::uint16_t res_avail = 0;
    std::uint8_t channel = 0;
    std::uint8_t rank = 0;
    std::array<std::uint8_t, 3> nibble_mask{};
    std::uint8_t bank_group = 0;
    std::uint8_t bank = 0;
    std::array<std::uint8_t, 3> row{};
    std::uint16_t column = 0;
    std::array<std::uint8_t, 16> component_id{};
    std::uint8_t sub_channel = 0;

    static constexpr std::uint8_t kRecordLength = 0x80;
    static constexpr std::size_t kPayloadOffset = 0x30;
    static constexpr std::size_t kPayloadBytes = 0x50;

    // Build a CxlEventRecord (payload-only view).  The common-header tail
    // fields (maint_op_class/ld_id/head_id) are not part of CxlEventRecord;
    // use build_raw() when those fields are needed.
    void build(CxlEventRecord* r) const
    {
        r->handle = handle;
        r->related = related;
        r->severity = severity;
        r->timestamp = timestamp;
        std::memcpy(r->uuid.data(), kCxlEventMemSparingUuid, 16);
        r->data.fill(0);
        r->data[0x02] = flags;
        r->data[0x03] = result;
        put16(r->data.data(), 0x04, validity_flags);
        put16(r->data.data(), 0x0C, res_avail);
        r->data[0x0E] = channel;
        r->data[0x0F] = rank;
        r->data[0x10] = nibble_mask[0];
        r->data[0x11] = nibble_mask[1];
        r->data[0x12] = nibble_mask[2];
        r->data[0x13] = bank_group;
        r->data[0x14] = bank;
        r->data[0x15] = row[0];
        r->data[0x16] = row[1];
        r->data[0x17] = row[2];
        put16(r->data.data(), 0x18, column);
        std::memcpy(r->data.data() + 0x1A, component_id.data(), 16);
        r->data[0x2A] = sub_channel;
    }

    // Full raw record (0x80 bytes) including the common header tail fields
    // that CxlEventRecord does not carry.  Use this when the host/CPER path
    // needs maint_op_class/ld_id/head_id.
    std::vector<std::uint8_t> build_raw() const
    {
        std::vector<std::uint8_t> v(kRecordLength, 0);
        std::memcpy(v.data(), kCxlEventMemSparingUuid, 16);
        v[0x10] = kRecordLength;
        v[0x11] = (std::uint8_t)(severity & 0x3);
        put16(v, 0x14, handle);
        put16(v, 0x16, related);
        put64(v, 0x18, timestamp);
        v[0x20] = maint_op_class;
        v[0x21] = maint_op_sub_class;
        put16(v, 0x22, ld_id);
        v[0x24] = head_id;
        v[0x30 + 0x02] = flags;
        v[0x30 + 0x03] = result;
        put16(v, 0x30 + 0x04, validity_flags);
        put16(v, 0x30 + 0x0C, res_avail);
        v[0x30 + 0x0E] = channel;
        v[0x30 + 0x0F] = rank;
        v[0x30 + 0x10] = nibble_mask[0];
        v[0x30 + 0x11] = nibble_mask[1];
        v[0x30 + 0x12] = nibble_mask[2];
        v[0x30 + 0x13] = bank_group;
        v[0x30 + 0x14] = bank;
        v[0x30 + 0x15] = row[0];
        v[0x30 + 0x16] = row[1];
        v[0x30 + 0x17] = row[2];
        put16(v, 0x30 + 0x18, column);
        std::memcpy(v.data() + 0x30 + 0x1A, component_id.data(), 16);
        v[0x30 + 0x2A] = sub_channel;
        return v;
    }

    static bool parse(const std::uint8_t* raw, std::size_t n,
                      CxlMemSparingEvent* out)
    {
        if (!raw || !out || n < kRecordLength) return false;
        if (std::memcmp(raw, kCxlEventMemSparingUuid, 16) != 0) return false;
        if (raw[0x10] != kRecordLength) return false;
        out->severity = (std::uint8_t)(raw[0x11] & 0x3);
        out->handle = get16(raw, 0x14);
        out->related = get16(raw, 0x16);
        out->timestamp = get64(raw, 0x18);
        out->maint_op_class = raw[0x20];
        out->maint_op_sub_class = raw[0x21];
        out->ld_id = get16(raw, 0x22);
        out->head_id = raw[0x24];
        out->flags = raw[0x30 + 0x02];
        out->result = raw[0x30 + 0x03];
        out->validity_flags = get16(raw, 0x30 + 0x04);
        out->res_avail = get16(raw, 0x30 + 0x0C);
        out->channel = raw[0x30 + 0x0E];
        out->rank = raw[0x30 + 0x0F];
        for (int i = 0; i < 3; ++i) out->nibble_mask[i] = raw[0x30 + 0x10 + i];
        out->bank_group = raw[0x30 + 0x13];
        out->bank = raw[0x30 + 0x14];
        for (int i = 0; i < 3; ++i) out->row[i] = raw[0x30 + 0x15 + i];
        out->column = get16(raw, 0x30 + 0x18);
        std::memcpy(out->component_id.data(), raw + 0x30 + 0x1A, 16);
        out->sub_channel = raw[0x30 + 0x2A];
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
    static void put64(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint64_t x)
    {
        for (int i = 0; i < 8; ++i)
            v[o + i] = (std::uint8_t)((x >> (8 * i)) & 0xFF);
    }
};

} // namespace cxl_tlm
