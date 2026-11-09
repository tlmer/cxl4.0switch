// -----------------------------------------------------------------------------
// cxl_host_ext.h -- host-side wrappers for the 2026-11-04 Type-3 commands.
//
//   Builds the mailbox payloads defined by:
//     * CXL Type-3 security (0x4400/0x4401/0x4500-0x4505), Linux 6.18
//       drivers/cxl/cxlmem.h:553-560 and security.c;
//     * Event Interrupt Policy (0x0102/0x0103), cxlmem.h:209-217;
//     * Get Log Caps / Clear / Populate Log (0x0402-0x0404);
//     * Get Supported Logs Sub-List (0x0405);
//     * Background Operation Status / Response Message Limit (0x0002-0x0004);
//     * Feature discovery (0x0500-0x0502), include/uapi/cxl/features.h;
//     * SLD QoS (0x4700-0x4702), Media Operation (0x4402) and the
//       optional DCD commands (0x4800-0x4803).
//
//   Same contract as cxl_host_mailbox.h: false only for transport/protocol
//   failures; a device return code is reported in out->rc with true.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_features.h"
#include "cxl_host_mailbox.h"
#include "cxl_security.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace cxl_tlm {

inline void cxl_host_ext_set_err(std::string* err, const char* msg)
{
    if (err) *err = msg;
}

// ---- security --------------------------------------------------------------
inline bool cxl_host_mbx_get_security_state(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint32_t* flags,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_SECURITY_STATE, {}, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.size() < 4) {
        cxl_host_ext_set_err(err, "get security state: short response");
        return false;
    }
    *flags = (std::uint32_t)r.response[0] | ((std::uint32_t)r.response[1] << 8) |
             ((std::uint32_t)r.response[2] << 16) |
             ((std::uint32_t)r.response[3] << 24);
    return true;
}

inline bool cxl_host_mbx_set_passphrase(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint8_t type,
    const std::vector<std::uint8_t>& old_pass,
    const std::vector<std::uint8_t>& new_pass,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (old_pass.size() != 32 || new_pass.size() != 32) {
        cxl_host_ext_set_err(err, "set passphrase: need 32-byte old/new");
        return false;
    }
    std::vector<std::uint8_t> v(0x60, 0);
    v[0] = type;
    std::memcpy(v.data() + 0x20, old_pass.data(), 32);
    std::memcpy(v.data() + 0x40, new_pass.data(), 32);
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_SET_PASSPHRASE, v, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

inline bool cxl_host_mbx_disable_passphrase(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint8_t type,
    const std::vector<std::uint8_t>& pass,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (pass.size() != 32) {
        cxl_host_ext_set_err(err, "disable passphrase: need 32-byte pass");
        return false;
    }
    std::vector<std::uint8_t> v(0x40, 0);
    v[0] = type;
    std::memcpy(v.data() + 0x20, pass.data(), 32);
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_DISABLE_PASSPHRASE, v, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

inline bool cxl_host_mbx_unlock(const CxlHostMbxIo& io, std::uint64_t base,
                                const std::vector<std::uint8_t>& pass,
                                CxlHostMbxResult* out = nullptr,
                                std::string* err = nullptr)
{
    if (pass.size() != 32) {
        cxl_host_ext_set_err(err, "unlock: need 32-byte pass");
        return false;
    }
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_UNLOCK, pass, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

inline bool cxl_host_mbx_freeze_security(const CxlHostMbxIo& io,
                                         std::uint64_t base,
                                         CxlHostMbxResult* out = nullptr,
                                         std::string* err = nullptr)
{
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_FREEZE_SECURITY, {}, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

inline bool cxl_host_mbx_sanitize(const CxlHostMbxIo& io, std::uint64_t base,
                                  CxlHostMbxResult* out = nullptr,
                                  std::string* err = nullptr)
{
    // Sanitize is a background command: start it and read the immediate
    // return code from one status poll.  Do not use submit(), which waits for
    // the Background bit to clear.
    std::uint16_t psz = 0;
    if (!cxl_host_mbx_start(io, base, MBX_CMD_SANITIZE, {}, &psz, err))
        return false;
    CxlHostMbxBackground bg;
    std::string poll_err;
    (void)cxl_host_mbx_poll_background(io, base, &bg, 1, &poll_err);
    if (out) {
        *out = CxlHostMbxResult();
        out->payload_size = psz;
        out->rc = bg.rc;
    }
    return true;
}

inline bool cxl_host_mbx_secure_erase(const CxlHostMbxIo& io,
                                      std::uint64_t base,
                                      CxlHostMbxResult* out = nullptr,
                                      std::string* err = nullptr)
{
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_SECURE_ERASE, {}, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

inline bool cxl_host_mbx_passphrase_secure_erase(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint8_t type,
    const std::vector<std::uint8_t>& pass,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (pass.size() != 32) {
        cxl_host_ext_set_err(err, "passphrase erase: need 32-byte pass");
        return false;
    }
    std::vector<std::uint8_t> v(0x40, 0);
    v[0] = type;
    std::memcpy(v.data() + 0x20, pass.data(), 32);
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_PASSPHRASE_SECURE_ERASE, v,
                             &r, err))
        return false;
    if (out) *out = r;
    return true;
}

// ---- event interrupt policy ------------------------------------------------
inline bool cxl_host_mbx_get_event_policy(
    const CxlHostMbxIo& io, std::uint64_t base,
    std::array<std::uint8_t, 4>* settings,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_EVT_INT_POLICY, {}, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.size() < 4) {
        cxl_host_ext_set_err(err, "event policy: short response");
        return false;
    }
    for (int i = 0; i < 4; ++i) (*settings)[(std::size_t)i] = r.response[(std::size_t)i];
    return true;
}

inline bool cxl_host_mbx_set_event_policy(
    const CxlHostMbxIo& io, std::uint64_t base,
    const std::array<std::uint8_t, 4>& settings,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    std::vector<std::uint8_t> v(settings.begin(), settings.end());
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_SET_EVT_INT_POLICY, v, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

// ---- log capabilities ------------------------------------------------------
inline bool cxl_host_mbx_get_log_caps(
    const CxlHostMbxIo& io, std::uint64_t base,
    const std::array<std::uint8_t, 16>& uuid, std::uint32_t* flags,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    std::vector<std::uint8_t> v(uuid.begin(), uuid.end());
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_LOG_CAPS, v, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.size() < 4) {
        cxl_host_ext_set_err(err, "log caps: short response");
        return false;
    }
    *flags = (std::uint32_t)r.response[0] | ((std::uint32_t)r.response[1] << 8) |
             ((std::uint32_t)r.response[2] << 16) |
             ((std::uint32_t)r.response[3] << 24);
    return true;
}

inline bool cxl_host_mbx_clear_log(const CxlHostMbxIo& io, std::uint64_t base,
                                   const std::array<std::uint8_t, 16>& uuid,
                                   CxlHostMbxResult* out = nullptr,
                                   std::string* err = nullptr)
{
    std::vector<std::uint8_t> v(uuid.begin(), uuid.end());
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_CLEAR_LOG, v, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

inline bool cxl_host_mbx_populate_log(
    const CxlHostMbxIo& io, std::uint64_t base,
    const std::array<std::uint8_t, 16>& uuid,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    std::vector<std::uint8_t> v(uuid.begin(), uuid.end());
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_POPULATE_LOG, v, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

// ---- SLD QoS telemetry -----------------------------------------------------
struct CxlHostSldQos {
    std::uint8_t control = 0;       // bit0 egress congestion, bit1 TTR
    std::uint8_t moderate_pct = 10;
    std::uint8_t severe_pct = 25;
    std::uint8_t sample_interval = 8;
};

inline bool cxl_host_mbx_get_sld_qos(
    const CxlHostMbxIo& io, std::uint64_t base, CxlHostSldQos* cfg,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (!cfg) {
        cxl_host_ext_set_err(err, "get SLD QoS: null output");
        return false;
    }
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_SLD_QOS_CTRL, {}, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.size() < 4) {
        cxl_host_ext_set_err(err, "get SLD QoS: short response");
        return false;
    }
    cfg->control = r.response[0];
    cfg->moderate_pct = r.response[1];
    cfg->severe_pct = r.response[2];
    cfg->sample_interval = r.response[3];
    return true;
}

inline bool cxl_host_mbx_set_sld_qos(
    const CxlHostMbxIo& io, std::uint64_t base, const CxlHostSldQos& cfg,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    const std::vector<std::uint8_t> v = {cfg.control, cfg.moderate_pct,
                                         cfg.severe_pct, cfg.sample_interval};
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_SET_SLD_QOS_CTRL, v, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

inline bool cxl_host_mbx_get_sld_qos_status(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint8_t* avg_pct,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (!avg_pct) {
        cxl_host_ext_set_err(err, "SLD QoS status: null output");
        return false;
    }
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_SLD_QOS_STATUS, {}, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.empty()) {
        cxl_host_ext_set_err(err, "SLD QoS status: empty response");
        return false;
    }
    *avg_pct = r.response[0];
    return true;
}

// ---- media operations (0x4402) -------------------------------------------
struct CxlHostMediaOpCaps {
    std::uint64_t granularity = 0;
    std::uint16_t total = 0;
    std::vector<std::pair<std::uint8_t, std::uint8_t>> ops;   // class, subclass
};

inline bool cxl_host_mbx_media_op_discovery(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint16_t start,
    std::uint16_t want, CxlHostMediaOpCaps* caps,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (!caps) {
        cxl_host_ext_set_err(err, "media discovery: null output");
        return false;
    }
    std::vector<std::uint8_t> v(0x0C, 0);
    v[0] = 0;                       // General class
    v[1] = 0;                       // Discovery
    v[0x08] = (std::uint8_t)(start & 0xFF);
    v[0x09] = (std::uint8_t)(start >> 8);
    v[0x0A] = (std::uint8_t)(want & 0xFF);
    v[0x0B] = (std::uint8_t)(want >> 8);
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_MEDIA_OPERATION, v, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.size() < 0x0C) {
        cxl_host_ext_set_err(err, "media discovery: short response");
        return false;
    }
    caps->granularity = 0;
    for (int i = 0; i < 8; ++i)
        caps->granularity |= (std::uint64_t)r.response[(std::size_t)i] << (8 * i);
    caps->total = (std::uint16_t)(r.response[8] | (r.response[9] << 8));
    const std::uint16_t n =
        (std::uint16_t)(r.response[10] | (r.response[11] << 8));
    if (r.response.size() < 0x0C + (std::size_t)n * 2) {
        cxl_host_ext_set_err(err, "media discovery: truncated list");
        return false;
    }
    caps->ops.clear();
    for (std::uint16_t i = 0; i < n; ++i)
        caps->ops.emplace_back(r.response[0x0C + i * 2],
                               r.response[0x0D + i * 2]);
    return true;
}

inline bool cxl_host_mbx_media_operation(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint8_t cls,
    std::uint8_t sub,
    const std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    std::vector<std::uint8_t> v(8 + ranges.size() * 16, 0);
    v[0] = cls;
    v[1] = sub;
    const std::uint32_t n = (std::uint32_t)ranges.size();
    for (int i = 0; i < 4; ++i) v[4 + (std::size_t)i] =
        (std::uint8_t)(n >> (8 * i));
    for (std::size_t k = 0; k < ranges.size(); ++k) {
        for (int i = 0; i < 8; ++i) {
            v[8 + k * 16 + (std::size_t)i] =
                (std::uint8_t)(ranges[k].first >> (8 * i));
            v[8 + k * 16 + 8 + (std::size_t)i] =
                (std::uint8_t)(ranges[k].second >> (8 * i));
        }
    }
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_MEDIA_OPERATION, v, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

// ---- Dynamic Capacity (DCD, opt-in profile) -------------------------------
inline std::uint32_t cxl_host_dcd_rd32(const std::vector<std::uint8_t>& b,
                                       std::size_t o)
{
    return (std::uint32_t)b[o] | ((std::uint32_t)b[o + 1] << 8) |
           ((std::uint32_t)b[o + 2] << 16) |
           ((std::uint32_t)b[o + 3] << 24);
}
inline std::uint64_t cxl_host_dcd_rd64(const std::vector<std::uint8_t>& b,
                                       std::size_t o)
{
    std::uint64_t x = 0;
    for (int i = 0; i < 8; ++i)
        x |= (std::uint64_t)b[o + (std::size_t)i] << (8 * i);
    return x;
}

struct CxlHostDcdRegion {
    std::uint64_t base = 0, decode_len = 0, region_len = 0, block_size = 0;
    std::uint32_t dsmad_handle = 0;
    std::uint8_t flags = 0;
};
struct CxlHostDcdExtent {
    std::uint64_t dpa = 0, len = 0;
    std::array<std::uint8_t, 16> tag{};
    std::uint16_t seq = 0;
};

inline bool cxl_host_mbx_get_dc_config(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint8_t start,
    std::uint8_t count, std::vector<CxlHostDcdRegion>* regs,
    std::uint32_t* total_extents = nullptr,
    std::uint32_t* avail_extents = nullptr,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (!regs) {
        cxl_host_ext_set_err(err, "DCD config: null output");
        return false;
    }
    const std::vector<std::uint8_t> v = {count, start};
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_DC_CONFIG, v, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    const std::vector<std::uint8_t>& b = r.response;
    if (b.size() < 8) {
        cxl_host_ext_set_err(err, "DCD config: short header");
        return false;
    }
    const std::uint8_t n = b[1];
    if (b.size() < 8u + (std::size_t)n * 0x28u + 16u) {
        cxl_host_ext_set_err(err, "DCD config: truncated regions");
        return false;
    }
    regs->clear();
    for (std::uint8_t i = 0; i < n; ++i) {
        const std::size_t o = 8 + (std::size_t)i * 0x28;
        CxlHostDcdRegion g;
        g.base = cxl_host_dcd_rd64(b, o);
        g.decode_len = cxl_host_dcd_rd64(b, o + 8);
        g.region_len = cxl_host_dcd_rd64(b, o + 0x10);
        g.block_size = cxl_host_dcd_rd64(b, o + 0x18);
        g.dsmad_handle = cxl_host_dcd_rd32(b, o + 0x20);
        g.flags = b[o + 0x24];
        regs->push_back(g);
    }
    const std::size_t tail = 8 + (std::size_t)n * 0x28;
    if (total_extents) *total_extents = cxl_host_dcd_rd32(b, tail);
    if (avail_extents) *avail_extents = cxl_host_dcd_rd32(b, tail + 4);
    return true;
}

inline bool cxl_host_mbx_get_dc_extents(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint32_t start,
    std::uint32_t count, std::vector<CxlHostDcdExtent>* extents,
    std::uint32_t* total = nullptr, std::uint32_t* generation = nullptr,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (!extents) {
        cxl_host_ext_set_err(err, "DCD extents: null output");
        return false;
    }
    std::vector<std::uint8_t> v(8, 0);
    for (int i = 0; i < 4; ++i) {
        v[i] = (std::uint8_t)(count >> (8 * i));
        v[4 + i] = (std::uint8_t)(start >> (8 * i));
    }
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_DC_EXTENTS, v, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    const std::vector<std::uint8_t>& b = r.response;
    if (b.size() < 0x10) {
        cxl_host_ext_set_err(err, "DCD extents: short header");
        return false;
    }
    const std::uint32_t n = cxl_host_dcd_rd32(b, 0);
    if (b.size() < 0x10u + (std::size_t)n * 0x28u) {
        cxl_host_ext_set_err(err, "DCD extents: truncated list");
        return false;
    }
    if (total) *total = cxl_host_dcd_rd32(b, 4);
    if (generation) *generation = cxl_host_dcd_rd32(b, 8);
    extents->clear();
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::size_t o = 0x10 + (std::size_t)i * 0x28;
        CxlHostDcdExtent e;
        e.dpa = cxl_host_dcd_rd64(b, o);
        e.len = cxl_host_dcd_rd64(b, o + 8);
        std::memcpy(e.tag.data(), b.data() + o + 0x10, 16);
        e.seq = (std::uint16_t)(b[o + 0x20] | (b[o + 0x21] << 8));
        extents->push_back(e);
    }
    return true;
}

inline std::vector<std::uint8_t> cxl_host_dcd_payload(
    const std::vector<CxlHostDcdExtent>& extents, bool more)
{
    std::vector<std::uint8_t> v(8 + extents.size() * 40, 0);
    const std::uint32_t n = (std::uint32_t)extents.size();
    for (int i = 0; i < 4; ++i) v[i] = (std::uint8_t)(n >> (8 * i));
    v[4] = more ? 0x01 : 0x00;
    for (std::size_t i = 0; i < extents.size(); ++i) {
        const std::size_t o = 8 + i * 40;
        for (int j = 0; j < 8; ++j) {
            v[o + (std::size_t)j] =
                (std::uint8_t)(extents[i].dpa >> (8 * j));
            v[o + 8 + (std::size_t)j] =
                (std::uint8_t)(extents[i].len >> (8 * j));
        }
        std::memcpy(&v[o + 0x10], extents[i].tag.data(), 16);
        v[o + 0x20] = (std::uint8_t)(extents[i].seq & 0xFF);
        v[o + 0x21] = (std::uint8_t)(extents[i].seq >> 8);
    }
    return v;
}

inline bool cxl_host_mbx_add_dc_response(
    const CxlHostMbxIo& io, std::uint64_t base,
    const std::vector<CxlHostDcdExtent>& extents, bool more = false,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_ADD_DC_RESPONSE,
                             cxl_host_dcd_payload(extents, more), &r, err))
        return false;
    if (out) *out = r;
    return true;
}

inline bool cxl_host_mbx_release_dc(
    const CxlHostMbxIo& io, std::uint64_t base,
    const std::vector<CxlHostDcdExtent>& extents, bool more = false,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_RELEASE_DC,
                             cxl_host_dcd_payload(extents, more), &r, err))
        return false;
    if (out) *out = r;
    return true;
}

// ---- generic status / response limit --------------------------------------
struct CxlHostBgStatus {
    bool active = false;
    std::uint8_t percent = 0;
    std::uint16_t opcode = 0;
    std::uint16_t rc = 0;
    std::uint16_t ext_status = 0;
};

inline bool cxl_host_mbx_get_bg_status(
    const CxlHostMbxIo& io, std::uint64_t base, CxlHostBgStatus* st,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (!st) {
        cxl_host_ext_set_err(err, "background status: null output");
        return false;
    }
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_BG_STATUS, {}, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.size() < 8) {
        cxl_host_ext_set_err(err, "background status: short response");
        return false;
    }
    st->active = (r.response[0] & 0x01) != 0;
    st->percent = (std::uint8_t)((r.response[0] >> 1) & 0x7F);
    st->opcode = (std::uint16_t)(r.response[2] | (r.response[3] << 8));
    st->rc = (std::uint16_t)(r.response[4] | (r.response[5] << 8));
    st->ext_status = (std::uint16_t)(r.response[6] | (r.response[7] << 8));
    return true;
}

inline bool cxl_host_mbx_get_response_limit(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint8_t* exponent,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    if (!exponent) {
        cxl_host_ext_set_err(err, "get response limit: null output");
        return false;
    }
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_RESP_LIMIT, {}, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.empty()) {
        cxl_host_ext_set_err(err, "get response limit: empty response");
        return false;
    }
    *exponent = r.response[0];
    return true;
}

inline bool cxl_host_mbx_set_response_limit(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint8_t exponent,
    std::uint8_t* applied = nullptr, CxlHostMbxResult* out = nullptr,
    std::string* err = nullptr)
{
    const std::vector<std::uint8_t> v = {exponent};
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_SET_RESP_LIMIT, v, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc == MBX_RC_SUCCESS && applied) {
        if (r.response.empty()) {
            cxl_host_ext_set_err(err, "set response limit: empty response");
            return false;
        }
        *applied = r.response[0];
    }
    return true;
}

// ---- features --------------------------------------------------------------
struct CxlHostFeatEntry {
    std::array<std::uint8_t, 16> uuid{};
    std::uint16_t id = 0;
    std::uint16_t get_size = 0;
    std::uint16_t set_size = 0;
    std::uint32_t flags = 0;
    std::uint8_t get_ver = 0;
    std::uint8_t set_ver = 0;
    std::uint16_t effects = 0;
};

inline std::uint16_t cxl_host_rd16(const std::vector<std::uint8_t>& v,
                                   std::size_t o)
{
    return (std::uint16_t)(v[o] | (v[o + 1] << 8));
}
inline std::uint32_t cxl_host_rd32(const std::vector<std::uint8_t>& v,
                                   std::size_t o)
{
    return (std::uint32_t)v[o] | ((std::uint32_t)v[o + 1] << 8) |
           ((std::uint32_t)v[o + 2] << 16) |
           ((std::uint32_t)v[o + 3] << 24);
}

inline bool cxl_host_mbx_get_supported_features(
    const CxlHostMbxIo& io, std::uint64_t base, std::uint16_t start_idx,
    std::uint32_t count_bytes, std::vector<CxlHostFeatEntry>* entries,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    std::vector<std::uint8_t> v(8, 0);
    v[0] = (std::uint8_t)(count_bytes & 0xFF);
    v[1] = (std::uint8_t)((count_bytes >> 8) & 0xFF);
    v[2] = (std::uint8_t)((count_bytes >> 16) & 0xFF);
    v[3] = (std::uint8_t)((count_bytes >> 24) & 0xFF);
    v[4] = (std::uint8_t)(start_idx & 0xFF);
    v[5] = (std::uint8_t)(start_idx >> 8);
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_SUPPORTED_FEATURES, v, &r,
                             err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    if (r.response.size() < 8) {
        cxl_host_ext_set_err(err, "supported features: short header");
        return false;
    }
    const std::uint16_t n = cxl_host_rd16(r.response, 0);
    if (entries) entries->clear();
    if (r.response.size() < 8u + (std::size_t)n * 48u) {
        cxl_host_ext_set_err(err, "supported features: short entry array");
        return false;
    }
    for (std::uint16_t i = 0; i < n && entries; ++i) {
        const std::size_t o = 8u + (std::size_t)i * 48u;
        CxlHostFeatEntry e;
        std::memcpy(e.uuid.data(), r.response.data() + o, 16);
        e.id = cxl_host_rd16(r.response, o + 0x10);
        e.get_size = cxl_host_rd16(r.response, o + 0x12);
        e.set_size = cxl_host_rd16(r.response, o + 0x14);
        e.flags = cxl_host_rd32(r.response, o + 0x16);
        e.get_ver = r.response[o + 0x1A];
        e.set_ver = r.response[o + 0x1B];
        e.effects = cxl_host_rd16(r.response, o + 0x1C);
        entries->push_back(e);
    }
    return true;
}

inline bool cxl_host_mbx_get_feature(
    const CxlHostMbxIo& io, std::uint64_t base,
    const std::array<std::uint8_t, 16>& uuid, std::uint16_t offset,
    std::uint16_t count, std::uint8_t selection,
    std::vector<std::uint8_t>* data, CxlHostMbxResult* out = nullptr,
    std::string* err = nullptr)
{
    std::vector<std::uint8_t> v(21, 0);
    std::memcpy(v.data(), uuid.data(), 16);
    v[16] = (std::uint8_t)(offset & 0xFF);
    v[17] = (std::uint8_t)(offset >> 8);
    v[18] = (std::uint8_t)(count & 0xFF);
    v[19] = (std::uint8_t)(count >> 8);
    v[20] = selection;
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_GET_FEATURE, v, &r, err))
        return false;
    if (out) *out = r;
    if (r.rc != MBX_RC_SUCCESS) return true;
    // The mailbox returns a zero-padded payload_size buffer; the feature
    // payload is exactly min(count, feature size - offset).  Expose the
    // requested slice, not the padding.
    if (data) {
        const std::size_t n = std::min<std::size_t>(count, r.response.size());
        data->assign(r.response.begin(), r.response.begin() + n);
    }
    return true;
}

inline bool cxl_host_mbx_set_feature(
    const CxlHostMbxIo& io, std::uint64_t base,
    const std::array<std::uint8_t, 16>& uuid, std::uint32_t flags,
    std::uint16_t offset, std::uint8_t version,
    const std::vector<std::uint8_t>& data,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr)
{
    std::vector<std::uint8_t> v(32, 0);
    std::memcpy(v.data(), uuid.data(), 16);
    v[16] = (std::uint8_t)(flags & 0xFF);
    v[17] = (std::uint8_t)((flags >> 8) & 0xFF);
    v[18] = (std::uint8_t)((flags >> 16) & 0xFF);
    v[19] = (std::uint8_t)((flags >> 24) & 0xFF);
    v[20] = (std::uint8_t)(offset & 0xFF);
    v[21] = (std::uint8_t)(offset >> 8);
    v[22] = version;
    v.insert(v.end(), data.begin(), data.end());
    CxlHostMbxResult r;
    if (!cxl_host_mbx_submit(io, base, MBX_CMD_SET_FEATURE, v, &r, err))
        return false;
    if (out) *out = r;
    return true;
}

} // namespace cxl_tlm
