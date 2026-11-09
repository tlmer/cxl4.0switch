// -----------------------------------------------------------------------------
// cxl_event_cper.h -- bridge a model CXL event record into a CPER section.
//
//   CxlEventRecord::serialize() is UUID[16] + event[112] (0x80 bytes).  The
//   CPER event section uses the UUID as the section type and the 112-byte
//   event body without the UUID, wrapped by cxl_cper_event_rec's 32-byte
//   header (device ID + serial + validation bits).
//
//   Header-only so tests that do not use CPER need not link cxl_cper.cpp.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_commands.h"
#include "cxl_cper.h"

#include <cstring>
#include <string>
#include <vector>

namespace cxl_tlm {

// CXL event records carry an RFC-4122 UUID in network byte order, while a
// CPER section type is a GUID (the first three fields are little-endian, see
// Linux cper.h GUID_INIT).  Convert so a CPER consumer matches the section
// type against cxl_cper_guid_gen_media()/dram()/mem_module().
inline CxlCperGuid cxl_event_cper_guid_from_uuid(const std::uint8_t* u) {
    CxlCperGuid g{};
    g.b[0] = u[3];
    g.b[1] = u[2];
    g.b[2] = u[1];
    g.b[3] = u[0];
    g.b[4] = u[5];
    g.b[5] = u[4];
    g.b[6] = u[7];
    g.b[7] = u[6];
    for (int i = 8; i < 16; ++i) g.b[i] = u[i];
    return g;
}

inline std::vector<std::uint8_t> cxl_event_cper_record(
    const CxlEventRecord& rec, const CxlCperEventDevId& dev,
    std::uint64_t dev_serial, std::uint32_t validation_bits,
    std::uint32_t severity, std::uint64_t record_id,
    std::string* err = nullptr) {
    const std::vector<std::uint8_t> raw = rec.serialize();
    if (raw.size() != 16u + kCxlCperEventBodySize) {
        if (err) *err = "cxl_event_cper_record: unexpected serialized size";
        return {};
    }
    const CxlCperGuid section_type =
        cxl_event_cper_guid_from_uuid(raw.data());
    const std::vector<std::uint8_t> event(raw.begin() + 16, raw.end());
    std::vector<std::uint8_t> out;
    if (!cxl_cper_build_event(section_type, dev, validation_bits, dev_serial,
                              event, severity, rec.timestamp, record_id, &out,
                              err))
        return {};
    return out;
}

} // namespace cxl_tlm
