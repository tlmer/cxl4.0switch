// -----------------------------------------------------------------------------
// cxl_event_mem_module.h -- Memory Module Event Record (CXL 3.1/3.2).
//
//   CXL 3.1 section 8.2.9.2.1.3, Table 8-47; Linux 6.18
//   include/cxl/event.h (struct cxl_event_mem_module, struct
//   cxl_get_health_info) and drivers/cxl/cxlmem.h:630
//   (CXL_EVENT_MEM_MODULE_UUID).
//
//   Raw record (0x80 bytes):
//     0x00 uuid[16] = fe927475-dd59-4339-a586-79bab113b774
//     0x10 common event header (0x20):
//          length=0x80, flags[3] (severity [1:0]), handle, related,
//          timestamp, maint_op_class, maint_op_sub_class, ld_id, head_id,
//          reserved[11]
//     0x30 payload[0x50]:
//       0x00 event_type
//       0x01 health_info[18]:
//             health_status, media_status, add_status, life_used,
//             device_temp(2), dirty_shutdown_cnt(4),
//             cor_vol_err_cnt(4), cor_per_err_cnt(4)
//       0x13 validity_flags(2)
//       0x15 component_id[16]
//       0x25 event_sub_type
//       0x26 reserved[0x2A]
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

constexpr std::uint8_t kCxlEventMemModuleUuid[16] = {
    0xfe, 0x92, 0x74, 0x75, 0xdd, 0x59, 0x43, 0x39,
    0xa5, 0x86, 0x79, 0xba, 0xb1, 0x13, 0xb7, 0x74,
};

struct CxlHealthInfo {
    std::uint8_t health_status = 0;
    std::uint8_t media_status = 0;
    std::uint8_t add_status = 0;
    std::uint8_t life_used = 0;
    std::uint16_t device_temp = 0;
    std::uint32_t dirty_shutdown_cnt = 0;
    std::uint32_t cor_vol_err_cnt = 0;
    std::uint32_t cor_per_err_cnt = 0;
};

struct CxlMemModuleEvent {
    // Common event-record header.
    std::uint8_t severity = 0;
    std::uint16_t handle = 0;
    std::uint16_t related = 0;
    std::uint64_t timestamp = 0;
    std::uint8_t maint_op_class = 0;
    std::uint8_t maint_op_sub_class = 0;
    std::uint16_t ld_id = 0;
    std::uint8_t head_id = 0;

    // Mem Module payload.
    std::uint8_t event_type = 0;
    CxlHealthInfo health;
    std::uint16_t validity_flags = 0;
    std::array<std::uint8_t, 16> component_id{};
    std::uint8_t event_sub_type = 0;

    static constexpr std::uint8_t kRecordLength = 0x80;

    // CxlEventRecord payload-only view (common-header tail fields are not
    // representable; use build_raw() when they matter).
    void build(CxlEventRecord* r) const
    {
        r->handle = handle;
        r->related = related;
        r->severity = severity;
        r->timestamp = timestamp;
        std::memcpy(r->uuid.data(), kCxlEventMemModuleUuid, 16);
        r->data.fill(0);
        r->data[0x00] = event_type;
        r->data[0x01] = health.health_status;
        r->data[0x02] = health.media_status;
        r->data[0x03] = health.add_status;
        r->data[0x04] = health.life_used;
        put16(r->data.data(), 0x05, health.device_temp);
        put32(r->data.data(), 0x07, health.dirty_shutdown_cnt);
        put32(r->data.data(), 0x0B, health.cor_vol_err_cnt);
        put32(r->data.data(), 0x0F, health.cor_per_err_cnt);
        put16(r->data.data(), 0x13, validity_flags);
        std::memcpy(r->data.data() + 0x15, component_id.data(), 16);
        r->data[0x25] = event_sub_type;
    }

    std::vector<std::uint8_t> build_raw() const
    {
        std::vector<std::uint8_t> v(kRecordLength, 0);
        std::memcpy(v.data(), kCxlEventMemModuleUuid, 16);
        v[0x10] = kRecordLength;
        v[0x11] = (std::uint8_t)(severity & 0x3);
        put16(v, 0x14, handle);
        put16(v, 0x16, related);
        put64(v, 0x18, timestamp);
        v[0x20] = maint_op_class;
        v[0x21] = maint_op_sub_class;
        put16(v, 0x22, ld_id);
        v[0x24] = head_id;
        v[0x30] = event_type;
        v[0x31] = health.health_status;
        v[0x32] = health.media_status;
        v[0x33] = health.add_status;
        v[0x34] = health.life_used;
        put16(v, 0x35, health.device_temp);
        put32(v, 0x37, health.dirty_shutdown_cnt);
        put32(v, 0x3B, health.cor_vol_err_cnt);
        put32(v, 0x3F, health.cor_per_err_cnt);
        put16(v, 0x43, validity_flags);
        std::memcpy(v.data() + 0x45, component_id.data(), 16);
        v[0x55] = event_sub_type;
        return v;
    }

    static bool parse(const std::uint8_t* raw, std::size_t n,
                      CxlMemModuleEvent* out)
    {
        if (!raw || !out || n < kRecordLength) return false;
        if (std::memcmp(raw, kCxlEventMemModuleUuid, 16) != 0) return false;
        if (raw[0x10] != kRecordLength) return false;
        out->severity = (std::uint8_t)(raw[0x11] & 0x3);
        out->handle = get16(raw, 0x14);
        out->related = get16(raw, 0x16);
        out->timestamp = get64(raw, 0x18);
        out->maint_op_class = raw[0x20];
        out->maint_op_sub_class = raw[0x21];
        out->ld_id = get16(raw, 0x22);
        out->head_id = raw[0x24];
        out->event_type = raw[0x30];
        out->health.health_status = raw[0x31];
        out->health.media_status = raw[0x32];
        out->health.add_status = raw[0x33];
        out->health.life_used = raw[0x34];
        out->health.device_temp = get16(raw, 0x35);
        out->health.dirty_shutdown_cnt = get32(raw, 0x37);
        out->health.cor_vol_err_cnt = get32(raw, 0x3B);
        out->health.cor_per_err_cnt = get32(raw, 0x3F);
        out->validity_flags = get16(raw, 0x43);
        std::memcpy(out->component_id.data(), raw + 0x45, 16);
        out->event_sub_type = raw[0x55];
        return true;
    }

private:
    static std::uint16_t get16(const std::uint8_t* p, std::size_t o)
    {
        return (std::uint16_t)(p[o] | (p[o + 1] << 8));
    }
    static std::uint32_t get32(const std::uint8_t* p, std::size_t o)
    {
        return (std::uint32_t)p[o] | ((std::uint32_t)p[o + 1] << 8) |
               ((std::uint32_t)p[o + 2] << 16) |
               ((std::uint32_t)p[o + 3] << 24);
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
    static void put32(std::uint8_t* p, std::size_t o, std::uint32_t x)
    {
        p[o] = (std::uint8_t)(x & 0xFF);
        p[o + 1] = (std::uint8_t)((x >> 8) & 0xFF);
        p[o + 2] = (std::uint8_t)((x >> 16) & 0xFF);
        p[o + 3] = (std::uint8_t)((x >> 24) & 0xFF);
    }
    static void put16(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint16_t x)
    {
        v[o] = (std::uint8_t)(x & 0xFF);
        v[o + 1] = (std::uint8_t)((x >> 8) & 0xFF);
    }
    static void put32(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint32_t x)
    {
        v[o] = (std::uint8_t)(x & 0xFF);
        v[o + 1] = (std::uint8_t)((x >> 8) & 0xFF);
        v[o + 2] = (std::uint8_t)((x >> 16) & 0xFF);
        v[o + 3] = (std::uint8_t)((x >> 24) & 0xFF);
    }
    static void put64(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint64_t x)
    {
        for (int i = 0; i < 8; ++i)
            v[o + i] = (std::uint8_t)((x >> (8 * i)) & 0xFF);
    }
};

} // namespace cxl_tlm
