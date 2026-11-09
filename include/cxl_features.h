// -----------------------------------------------------------------------------
// cxl_features.h -- CXL Feature discovery (0x0500 / 0x0501 / 0x0502).
//
//   CXL 3.2 sections 8.2.9.6.1-8.2.9.6.3; Linux 6.18
//   include/uapi/cxl/features.h and drivers/cxl/core/features.c.
//
//   Payload layouts (all little-endian, packed):
//     Get Supported Features input (8 B):
//       u32 count      bytes of feature data requested in the output
//       u16 start_idx  0-based first Supported Feature Entry
//       u8[2] reserved
//     Get Supported Features output:
//       u16 num_entries
//       u16 supported_feats
//       u8[4] reserved
//       Entry[num_entries]     (48 B each)
//     Entry (48 B):
//       uuid[16] id(u16) get_feat_size(u16) set_feat_size(u16)
//       flags(u32) get_feat_ver(u8) set_feat_ver(u8) effects(u16)
//       reserved[18]
//     Get Feature input (21 B):
//       uuid[16] offset(u16) count(u16) selection(u8)
//     Set Feature input (32 B header + data):
//       uuid[16] flags(u32) offset(u16) version(u8) reserved[9] data[]
//
//   Set Feature flags: bits[2:0] = 0 full / 1 initiate / 2 continue /
//   3 finish / 4 abort; bit[3] = data saved across reset.  The transfer
//   flags are taken from Linux enum cxl_set_feat_flag_data_transfer.
//
//   Model choices (documented where the spec/headers do not pin a mailbox
//   return code):
//     * Unknown feature UUID or unsupported selection: Invalid Input (0x02).
//     * Setting a feature whose Changeable flag is clear: Invalid Input.
//     * Version mismatch: Invalid Input.
//     * A chunked transfer with an out-of-order offset: Invalid Input;
//       the staging buffer is discarded.
//     * Abort transfer: staging discarded, Success; the caller observes the
//       attempt through `set_aborted`.
//     * Get Feature offsets past the end: Invalid Input; a request crossing
//       the end returns the available suffix (count is a maximum).
//     * Get Supported Features always returns the header; entries are added
//       while they fit in min(count, mailbox payload).  start_idx beyond the
//       list returns zero entries with the true supported count.
//
//   This block owns the feature table only; the CEL advertisement and the
//   mailbox whitelist are the Type-3 command layer's responsibility.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_mailbox.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace cxl_tlm {

enum CxlFeatFlag : std::uint32_t {
    kFeatChangeable       = 0x00000001,   // UAPI CXL_FEATURE_F_CHANGEABLE
    kFeatPersistFwUpdate  = 0x00000010,
    kFeatDefaultSel       = 0x00000020,
    kFeatSavedSel         = 0x00000040,
};

enum CxlFeatSelection : std::uint8_t {
    kFeatSelCurrent = 0,
    kFeatSelDefault = 1,
    kFeatSelSaved   = 2,
};

enum CxlFeatTransfer : std::uint32_t {
    kFeatXferFull     = 0,
    kFeatXferInitiate = 1,
    kFeatXferContinue = 2,
    kFeatXferFinish   = 3,
    kFeatXferAbort    = 4,
};

struct CxlFeatEntry {
    std::array<std::uint8_t, 16> uuid{};
    std::uint16_t id = 0;
    std::uint16_t get_size = 0;
    std::uint16_t set_size = 0;
    std::uint32_t flags = 0;
    std::uint8_t get_ver = 1;
    std::uint8_t set_ver = 1;
    std::uint16_t effects = 0;
    std::vector<std::uint8_t> current;    // current value
    std::vector<std::uint8_t> def;        // default value
    std::vector<std::uint8_t> saved;      // saved-across-reset value
    // Optional live feature hooks.  `getter` produces the current/default/
    // saved payload for features whose Get and Set layouts differ (for
    // example Memory Sparing); `setter` validates and applies a committed
    // Set payload.  Both are optional and default to the staged value.
    std::function<std::vector<std::uint8_t>(std::uint8_t)> getter;
    std::function<bool(const std::vector<std::uint8_t>&)> setter;
    bool changeable() const { return (flags & kFeatChangeable) != 0; }
};

struct CxlFeatureTable {
    static constexpr std::size_t kEntryBytes = 48;
    static constexpr std::size_t kGetInBytes = 8;
    static constexpr std::size_t kGetFeatInBytes = 21;
    static constexpr std::size_t kSetHdrBytes = 32;

    std::vector<CxlFeatEntry> feats;

    // Chunked Set Feature staging.
    int xfer_idx = -1;
    std::vector<std::uint8_t> xfer_buf;

    std::uint64_t get_supported_calls = 0, get_calls = 0, set_calls = 0;
    std::uint64_t bad_payload = 0, unknown_feature = 0, bad_selection = 0;
    std::uint64_t set_rejected = 0, set_committed = 0, set_aborted = 0;

    std::size_t add(const std::array<std::uint8_t, 16>& uuid,
                    std::uint16_t get_size, std::uint16_t set_size,
                    std::uint32_t flags,
                    const std::vector<std::uint8_t>& def_value,
                    std::uint16_t effects = 0)
    {
        CxlFeatEntry e;
        e.uuid = uuid;
        e.id = (std::uint16_t)feats.size();
        e.get_size = get_size;
        e.set_size = set_size;
        e.flags = flags;
        e.effects = effects;
        e.def = def_value;
        e.current = def_value;
        e.saved = def_value;
        const std::size_t idx = feats.size();
        feats.push_back(std::move(e));
        return idx;
    }

    bool get_supported(const std::vector<std::uint8_t>& in,
                       std::vector<std::uint8_t>* out, std::uint16_t* rc,
                       std::size_t max_payload)
    {
        ++get_supported_calls;
        if (in.size() != kGetInBytes) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        const std::uint32_t count = ld32(in, 0);
        const std::uint16_t start = ld16(in, 4);

        out->assign(8, 0);
        put16(*out, 0, 0);                                  // num_entries
        put16(*out, 2, (std::uint16_t)feats.size());        // supported_feats

        const std::size_t room = std::min<std::size_t>(
            count, max_payload ? max_payload : count);
        if (room <= 8 || start >= feats.size()) {
            *rc = MBX_RC_SUCCESS;
            return true;
        }
        const std::size_t max_entries = (room - 8) / kEntryBytes;
        std::size_t n = std::min(max_entries, feats.size() - start);
        for (std::size_t k = 0; k < n; ++k) {
            const CxlFeatEntry& e = feats[start + k];
            std::vector<std::uint8_t> b(kEntryBytes, 0);
            std::memcpy(b.data(), e.uuid.data(), 16);
            put16(b, 0x10, e.id);
            put16(b, 0x12, e.get_size);
            put16(b, 0x14, e.set_size);
            put32(b, 0x16, e.flags);
            b[0x1A] = e.get_ver;
            b[0x1B] = e.set_ver;
            put16(b, 0x1C, e.effects);
            out->insert(out->end(), b.begin(), b.end());
        }
        put16(*out, 0, (std::uint16_t)n);
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool get_feature(const std::vector<std::uint8_t>& in,
                     std::vector<std::uint8_t>* out, std::uint16_t* rc)
    {
        ++get_calls;
        if (in.size() != kGetFeatInBytes) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        CxlFeatEntry* e = find(in.data());
        if (!e) {
            ++unknown_feature;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        const std::uint16_t off = ld16(in, 16);
        const std::uint16_t count = ld16(in, 18);
        const std::uint8_t sel = in[20];
        if (sel > kFeatSelSaved) {
            ++bad_selection;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        std::vector<std::uint8_t> live;
        if (e->getter) live = e->getter(sel);
        const std::vector<std::uint8_t>& data = e->getter ? live
            : (sel == kFeatSelDefault) ? e->def
            : (sel == kFeatSelSaved) ? e->saved
                                     : e->current;
        if (off > data.size()) {
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        const std::size_t n =
            std::min<std::size_t>(count, data.size() - off);
        out->assign(data.begin() + off, data.begin() + off + n);
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool set_feature(const std::vector<std::uint8_t>& in, std::uint16_t* rc)
    {
        ++set_calls;
        if (in.size() < kSetHdrBytes) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        CxlFeatEntry* e = find(in.data());
        if (!e) {
            ++unknown_feature;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        const std::uint32_t flags = ld32(in, 16);
        const std::uint16_t off = ld16(in, 20);
        const std::uint8_t ver = in[22];
        const std::uint32_t xfer = flags & 0x7u;
        const bool saved_across_reset = (flags & 0x8u) != 0;
        const std::vector<std::uint8_t> data(in.begin() + kSetHdrBytes,
                                             in.end());

        auto reject = [&](std::uint16_t code) {
            ++set_rejected;
            *rc = code;
            return true;
        };

        if (!e->changeable()) return reject(MBX_RC_INVALID_INPUT);
        if (ver != e->set_ver) return reject(MBX_RC_INVALID_INPUT);
        if (xfer > kFeatXferAbort) return reject(MBX_RC_INVALID_INPUT);

        if (xfer == kFeatXferAbort) {
            xfer_idx = -1;
            xfer_buf.clear();
            ++set_aborted;
            *rc = MBX_RC_SUCCESS;
            return true;
        }

        if (xfer == kFeatXferFull) {
            if (off != 0) return reject(MBX_RC_INVALID_INPUT);
            if (data.size() > e->set_size) {
                return reject(MBX_RC_INVALID_PAYLOAD_LEN);
            }
            if (e->setter && !e->setter(data))
                return reject(MBX_RC_INVALID_INPUT);
            e->current = data;
            commit_saved(e, saved_across_reset);
            ++set_committed;
            *rc = MBX_RC_SUCCESS;
            return true;
        }

        // Initiate / continue / finish build one staged value.
        if (xfer == kFeatXferInitiate) {
            if (off != 0) {
                xfer_idx = -1;
                xfer_buf.clear();
                return reject(MBX_RC_INVALID_INPUT);
            }
            xfer_idx = (int)e->id;
            xfer_buf = data;
        } else {
            if (xfer_idx != (int)e->id || off != xfer_buf.size()) {
                xfer_idx = -1;
                xfer_buf.clear();
                return reject(MBX_RC_INVALID_INPUT);
            }
            xfer_buf.insert(xfer_buf.end(), data.begin(), data.end());
        }
        if (xfer_buf.size() > e->set_size) {
            xfer_idx = -1;
            xfer_buf.clear();
            return reject(MBX_RC_INVALID_PAYLOAD_LEN);
        }
        if (xfer == kFeatXferFinish) {
            if (e->setter && !e->setter(xfer_buf)) {
                xfer_idx = -1;
                xfer_buf.clear();
                return reject(MBX_RC_INVALID_INPUT);
            }
            e->current = xfer_buf;
            commit_saved(e, saved_across_reset);
            xfer_idx = -1;
            xfer_buf.clear();
            ++set_committed;
        }
        *rc = MBX_RC_SUCCESS;
        return true;
    }

private:
    static void commit_saved(CxlFeatEntry* e, bool saved)
    {
        if (saved) e->saved = e->current;
    }

    CxlFeatEntry* find(const std::uint8_t* uuid)
    {
        for (auto& e : feats)
            if (std::memcmp(e.uuid.data(), uuid, 16) == 0) return &e;
        return nullptr;
    }

    static std::uint16_t ld16(const std::vector<std::uint8_t>& v, std::size_t o)
    {
        return (std::uint16_t)(v[o] | (v[o + 1] << 8));
    }
    static std::uint32_t ld32(const std::vector<std::uint8_t>& v, std::size_t o)
    {
        return (std::uint32_t)v[o] | ((std::uint32_t)v[o + 1] << 8) |
               ((std::uint32_t)v[o + 2] << 16) |
               ((std::uint32_t)v[o + 3] << 24);
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
};

} // namespace cxl_tlm
