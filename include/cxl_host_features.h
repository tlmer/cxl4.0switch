// -----------------------------------------------------------------------------
// cxl_host_features.h -- host helpers for CXL feature paging and chunked Set.
//
//   Get Supported Features is paged: the first request asks for the 8-byte
//   header (count = 8) to learn supported_feats, then the caller requests
//   48-byte entries from start_idx.  This helper performs that loop.
//
//   Set Feature supports full, initiate, continue and finish transfers
//   (Linux include/uapi/cxl/features.h): when the payload exceeds one chunk,
//   send initiate + continue(s) + finish with the offsets the device expects.
//   The saved-across-reset flag (BIT(3)) is preserved from `extra_flags`.
//
//   All device return codes are reported in out->rc; false only for host-side
//   argument or transport/protocol errors.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_host_ext.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

inline bool cxl_host_mbx_get_all_features(
    const CxlHostMbxIo& io, std::uint64_t base,
    std::vector<CxlHostFeatEntry>* entries,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr,
    std::uint16_t page_entries = 8)
{
    if (!entries) {
        if (err) *err = "get all features: null output";
        return false;
    }
    if (page_entries == 0) page_entries = 8;
    entries->clear();
    CxlHostMbxResult r;
    // Header-only request: count = 8 bytes.
    if (!cxl_host_mbx_get_supported_features(io, base, 0, 8, nullptr, &r,
                                             err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.size() < 8) {
        if (err) *err = "get all features: short header";
        return false;
    }
    const std::uint16_t total = cxl_host_rd16(r.response, 2);
    std::uint16_t start = 0;
    // Bound the loop: at least one iteration, never more than the advertised
    // count plus one (a device that stops making progress terminates).
    for (std::uint32_t guard = 0;
         start < total && guard <= (std::uint32_t)total + 1; ++guard) {
        const std::uint32_t count =
            8u + 48u * (std::uint32_t)page_entries;
        std::vector<CxlHostFeatEntry> page;
        if (!cxl_host_mbx_get_supported_features(io, base, start, count, &page,
                                                 &r, err))
            return false;
        if (out) *out = r;
        if (r.rc != MBX_RC_SUCCESS) return true;
        if (page.empty()) break;              // no progress: stop
        entries->insert(entries->end(), page.begin(), page.end());
        start = (std::uint16_t)(start + (std::uint16_t)page.size());
    }
    return true;
}

inline bool cxl_host_mbx_set_feature_chunked(
    const CxlHostMbxIo& io, std::uint64_t base,
    const std::array<std::uint8_t, 16>& uuid, std::uint8_t version,
    const std::vector<std::uint8_t>& data, std::uint32_t chunk_bytes,
    std::uint32_t extra_flags = 0,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (chunk_bytes == 0) {
        if (err) *err = "set feature chunked: zero chunk size";
        return false;
    }
    const std::uint32_t saved = extra_flags & 0x8u;   // saved across reset
    CxlHostMbxResult r;

    if (data.size() <= chunk_bytes) {
        if (!cxl_host_mbx_set_feature(io, base, uuid,
                                      kFeatXferFull | saved, 0, version, data,
                                      &r, err))
            return false;
        if (out) *out = r;
        return true;
    }

    std::size_t off = 0;
    std::vector<std::uint8_t> first(data.begin(),
                                    data.begin() + chunk_bytes);
    if (!cxl_host_mbx_set_feature(io, base, uuid,
                                  kFeatXferInitiate | saved, 0, version, first,
                                  &r, err))
        return false;
    if (r.rc != MBX_RC_SUCCESS) {
        if (out) *out = r;
        return true;
    }
    off = chunk_bytes;
    while (data.size() - off > chunk_bytes) {
        std::vector<std::uint8_t> chunk(data.begin() + off,
                                        data.begin() + off + chunk_bytes);
        if (!cxl_host_mbx_set_feature(
                io, base, uuid, kFeatXferContinue | saved,
                (std::uint16_t)off, version, chunk, &r, err))
            return false;
        if (r.rc != MBX_RC_SUCCESS) {
            if (out) *out = r;
            return true;
        }
        off += chunk_bytes;
    }
    std::vector<std::uint8_t> last(data.begin() + off, data.end());
    if (!cxl_host_mbx_set_feature(io, base, uuid, kFeatXferFinish | saved,
                                  (std::uint16_t)off, version, last, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

} // namespace cxl_tlm
