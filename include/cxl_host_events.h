// -----------------------------------------------------------------------------
// cxl_host_events.h -- host-side Get/Clear Event Records with typed parsing.
//
//   Get Event Records (0x0100) input is one byte (log class); output is the
//   Linux 6.18 struct cxl_get_event_payload:
//     0x00 flags (bit0 overflow, bit1 more records)
//     0x02 overflow_err_count (u16)
//     0x04 first_overflow_timestamp (u64)
//     0x0C last_overflow_timestamp (u64)
//     0x14 record_count (u16)
//     0x16 reserved[10]
//     0x20 records[] (0x80 bytes each)
//
//   Clear Event Records (0x0101) input:
//     0x00 log class, 0x01 clear_flags (bit0 = clear all),
//     0x02 nr_recs, 0x03 reserved[3], 0x06 handles[u16]
//
//   Records are parsed with cxl_event_parse_any (General Media / DRAM /
//   Memory Module / Memory Sparing).  Unknown records are retained in
//   raw_records but not added to `events`; their count is exposed as
//   `unknown_records`.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_event_registry.h"
#include "cxl_host_mailbox.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlHostEventLog {
    std::uint8_t flags = 0;
    std::uint16_t overflow_count = 0;
    std::uint64_t first_overflow_timestamp = 0;
    std::uint64_t last_overflow_timestamp = 0;
    std::uint16_t record_count = 0;
    std::size_t unknown_records = 0;
    std::vector<std::vector<std::uint8_t>> raw_records;  // each 0x80
    std::vector<CxlEventAny> events;                     // recognized types
};

inline std::uint16_t cxl_host_ev_rd16(const std::vector<std::uint8_t>& v,
                                      std::size_t o)
{
    return (std::uint16_t)(v[o] | (v[o + 1] << 8));
}
inline std::uint64_t cxl_host_ev_rd64(const std::vector<std::uint8_t>& v,
                                      std::size_t o)
{
    std::uint64_t x = 0;
    for (int i = 0; i < 8; ++i) x |= (std::uint64_t)v[o + i] << (8 * i);
    return x;
}

inline bool cxl_host_mbx_get_event_records(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint8_t log_class,
    CxlHostEventLog* out_log, CxlHostMbxResult* out = nullptr,
    std::string* err = nullptr)
{
    if (!out_log) {
        if (err) *err = "get event records: null output";
        return false;
    }
    std::vector<std::uint8_t> req(1, log_class);
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_EVENT_REC, req, &r, err))
        return false;
    if (out) *out = r;
    *out_log = CxlHostEventLog();
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.size() < 0x20) {
        if (err) *err = "get event records: short header";
        return false;
    }
    out_log->flags = r.response[0x00];
    out_log->overflow_count = cxl_host_ev_rd16(r.response, 0x02);
    out_log->first_overflow_timestamp = cxl_host_ev_rd64(r.response, 0x04);
    out_log->last_overflow_timestamp = cxl_host_ev_rd64(r.response, 0x0C);
    out_log->record_count = cxl_host_ev_rd16(r.response, 0x14);
    const std::size_t need =
        0x20u + (std::size_t)out_log->record_count * 0x80u;
    if (r.response.size() < need) {
        if (err) *err = "get event records: short record array";
        return false;
    }
    for (std::size_t i = 0; i < out_log->record_count; ++i) {
        const std::uint8_t* p = r.response.data() + 0x20 + i * 0x80;
        out_log->raw_records.emplace_back(p, p + 0x80);
        CxlEventAny any;
        if (cxl_event_parse_any(p, 0x80, &any))
            out_log->events.push_back(any);
        else
            ++out_log->unknown_records;
    }
    return true;
}

inline bool cxl_host_mbx_clear_event_records(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint8_t log_class,
    bool clear_all, const std::vector<std::uint16_t>& handles,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (!clear_all && handles.empty()) {
        if (err) *err = "clear event records: no handles and not clear-all";
        return false;
    }
    std::vector<std::uint8_t> req(6, 0);
    req[0] = log_class;
    req[1] = clear_all ? 0x01 : 0x00;
    req[2] = (std::uint8_t)handles.size();
    for (std::uint16_t h : handles) {
        req.push_back((std::uint8_t)(h & 0xFF));
        req.push_back((std::uint8_t)(h >> 8));
    }
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_CLR_EVENT_REC, req, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

} // namespace cxl_tlm
