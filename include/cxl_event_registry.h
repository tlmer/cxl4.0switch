// -----------------------------------------------------------------------------
// cxl_event_registry.h -- UUID dispatch across the CXL event record types.
//
//   Recognizes the four CXL 3.1/3.2 media-related event records modeled in:
//     cxl_event_gen_media.h    General Media Event Record
//     cxl_event_dram.h         DRAM Event Record
//     cxl_event_mem_module.h   Memory Module Event Record
//     cxl_event_mser.h         Memory Sparing Event Record
//   plus the CXL 4.0 Dynamic Capacity Event Record (class 4):
//     cxl_event_dcd.h          Dynamic Capacity Event Record
//
//   Each parser enforces the 0x80-byte record length and the exact UUID.
//   Unknown UUIDs return CxlEventType::Unknown and parse_any returns false;
//   nothing is guessed from the payload shape.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_event_dcd.h"
#include "cxl_event_dram.h"
#include "cxl_event_gen_media.h"
#include "cxl_event_mem_module.h"
#include "cxl_event_mser.h"

#include <cstdint>
#include <cstring>

namespace cxl_tlm {

enum class CxlEventType {
    Unknown = 0,
    GenMedia,
    Dram,
    MemModule,
    MemSparing,
    Dcd,
};

inline CxlEventType cxl_event_type_of(const std::uint8_t* raw, std::size_t n)
{
    if (!raw || n < 16) return CxlEventType::Unknown;
    if (std::memcmp(raw, kCxlEventGenMediaUuid, 16) == 0)
        return CxlEventType::GenMedia;
    if (std::memcmp(raw, kCxlEventDramUuid, 16) == 0)
        return CxlEventType::Dram;
    if (std::memcmp(raw, kCxlEventMemModuleUuid, 16) == 0)
        return CxlEventType::MemModule;
    if (std::memcmp(raw, kCxlEventMemSparingUuid, 16) == 0)
        return CxlEventType::MemSparing;
    if (std::memcmp(raw, kCxlEventDcdUuid, 16) == 0)
        return CxlEventType::Dcd;
    return CxlEventType::Unknown;
}

inline const char* cxl_event_type_name(CxlEventType t)
{
    switch (t) {
    case CxlEventType::GenMedia:   return "General Media";
    case CxlEventType::Dram:       return "DRAM";
    case CxlEventType::MemModule:  return "Memory Module";
    case CxlEventType::MemSparing: return "Memory Sparing";
    case CxlEventType::Dcd:        return "Dynamic Capacity";
    default:                       return "Unknown";
    }
}

struct CxlEventAny {
    CxlEventType type = CxlEventType::Unknown;
    CxlGenMediaEvent gen_media;
    CxlDramEvent dram;
    CxlMemModuleEvent mem_module;
    CxlMemSparingEvent mem_sparing;
    CxlDcdEvent dcd;
};

inline bool cxl_event_parse_any(const std::uint8_t* raw, std::size_t n,
                                CxlEventAny* out)
{
    if (!out) return false;
    *out = CxlEventAny();
    out->type = cxl_event_type_of(raw, n);
    switch (out->type) {
    case CxlEventType::GenMedia:
        return CxlGenMediaEvent::parse(raw, n, &out->gen_media);
    case CxlEventType::Dram:
        return CxlDramEvent::parse(raw, n, &out->dram);
    case CxlEventType::MemModule:
        return CxlMemModuleEvent::parse(raw, n, &out->mem_module);
    case CxlEventType::MemSparing:
        return CxlMemSparingEvent::parse(raw, n, &out->mem_sparing);
    case CxlEventType::Dcd:
        return CxlDcdEvent::parse(raw, n, &out->dcd);
    default:
        return false;
    }
}

} // namespace cxl_tlm
