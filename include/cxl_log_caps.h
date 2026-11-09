// -----------------------------------------------------------------------------
// cxl_log_caps.h -- Get Log Capabilities (0x0402) / Clear Log (0x0403).
//
//   CXL 3.2 section 8.2.10.5.3 (older 3.1 section 8.2.9.5.3); Linux 6.18
//   drivers/cxl/core/mbox.c:61-62 registers both commands with a 16-byte
//   input (the Log Identifier UUID) and, for Get Log Capabilities, a 4-byte
//   output (Parameter Flags):
//
//     bit[0] Clear Log Supported
//     bit[1] Populate Log Supported
//     bit[2] Auto Populate Supported (log-specific meaning)
//     bit[3] Persistent Across Cold Reset
//     bits[31:4] reserved
//
//   Unknown Log Identifier UUIDs are rejected with Invalid Log (0x17), never
//   silently treated as a supported log.  Clear Log on a known log that does
//   not advertise Clear Log Supported is Invalid Input (0x02).
//
//   Populate Log (0x0404, CXL 4.0 Table 8-271) is synchronous here: a known
//   log without Populate Log Supported or without a callback is Invalid
//   Input; an unknown UUID is Invalid Log.
//
//   The table is owned by the caller: the Type-3 command layer registers the
//   Command Effects Log (CEL) as read-only (flags = 0, clearable = false) and
//   may register additional logs with clear/populate callbacks.  This header
//   owns no log contents.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_mailbox.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace cxl_tlm {

enum CxlLogCapFlag : std::uint32_t {
    kLogCapClearSupported     = 0x00000001,
    kLogCapPopulateSupported  = 0x00000002,
    kLogCapAutoPopulate       = 0x00000004,
    kLogCapPersistent         = 0x00000008,
};

struct CxlLogCaps {
    static constexpr std::size_t kUuidBytes = 16;

    struct LogEntry {
        std::array<std::uint8_t, kUuidBytes> uuid{};
        std::uint32_t flags = 0;
        std::uint32_t size = 0;           // maximum bytes of log data
        std::function<void()> clear;      // absent => Clear Log unsupported
        std::function<void()> populate;   // absent => Populate Log unsupported
        std::string name;
    };

    std::vector<LogEntry> logs;

    std::uint64_t get_caps_ok = 0, get_caps_invalid = 0;
    std::uint64_t clear_ok = 0, clear_invalid = 0;
    std::uint64_t populate_ok = 0, populate_invalid = 0;
    std::uint64_t bad_payload = 0;
    std::uint64_t sub_list_ok = 0, sub_list_invalid = 0;

    void add_log(const std::array<std::uint8_t, kUuidBytes>& uuid,
                 std::uint32_t flags,
                 std::function<void()> clear,
                 const std::string& name,
                 std::uint32_t size = 0,
                 std::function<void()> populate = nullptr)
    {
        LogEntry e;
        e.uuid = uuid;
        e.flags = flags;
        e.size = size;
        e.clear = std::move(clear);
        e.populate = std::move(populate);
        e.name = name;
        logs.push_back(std::move(e));
    }

    bool get_caps(const std::vector<std::uint8_t>& in,
                  std::vector<std::uint8_t>* out, std::uint16_t* rc)
    {
        if (in.size() != kUuidBytes) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        const LogEntry* e = find(in);
        if (!e) {
            ++get_caps_invalid;
            *rc = MBX_RC_INVALID_LOG;
            return true;
        }
        out->assign(4, 0);
        (*out)[0] = (std::uint8_t)(e->flags & 0xFF);
        (*out)[1] = (std::uint8_t)((e->flags >> 8) & 0xFF);
        (*out)[2] = (std::uint8_t)((e->flags >> 16) & 0xFF);
        (*out)[3] = (std::uint8_t)((e->flags >> 24) & 0xFF);
        ++get_caps_ok;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    // Get Supported Logs Sub-List (0x0405, CXL 4.0 Table 8-272/8-273).
    // Input: { u8 max_entries (>=1), u8 start_index }.
    // Output: { u8 count, u8 rsvd, u16 total, u8 start, u8 rsvd[3],
    //           entries[] { uuid[16], u32 size } }.
    bool sub_list(const std::vector<std::uint8_t>& in,
                  std::vector<std::uint8_t>* out, std::uint16_t* rc)
    {
        if (in.size() != 2) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        const std::uint8_t max_entries = in[0];
        const std::uint8_t start = in[1];
        const std::size_t total = logs.size();
        if (max_entries == 0 || (std::size_t)start > total) {
            ++sub_list_invalid;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        std::size_t count = total - start;
        if (count > max_entries) count = max_entries;
        out->assign(8 + count * 20, 0);
        (*out)[0] = (std::uint8_t)count;
        (*out)[2] = (std::uint8_t)(total & 0xFF);
        (*out)[3] = (std::uint8_t)((total >> 8) & 0xFF);
        (*out)[4] = start;
        for (std::size_t i = 0; i < count; ++i) {
            const LogEntry& e = logs[start + i];
            const std::size_t off = 8 + i * 20;
            std::copy(e.uuid.begin(), e.uuid.end(), out->begin() + off);
            (*out)[off + 16] = (std::uint8_t)(e.size & 0xFF);
            (*out)[off + 17] = (std::uint8_t)((e.size >> 8) & 0xFF);
            (*out)[off + 18] = (std::uint8_t)((e.size >> 16) & 0xFF);
            (*out)[off + 19] = (std::uint8_t)((e.size >> 24) & 0xFF);
        }
        ++sub_list_ok;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool clear_log(const std::vector<std::uint8_t>& in, std::uint16_t* rc)
    {
        if (in.size() != kUuidBytes) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        LogEntry* e = find(in);
        if (!e) {
            ++clear_invalid;
            *rc = MBX_RC_INVALID_LOG;
            return true;
        }
        // CXL 4.0 8.2.10.5.4: a known log without Clear Log Supported is
        // Invalid Input (not Invalid Log).
        if (!(e->flags & kLogCapClearSupported) || !e->clear) {
            ++clear_invalid;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        e->clear();
        ++clear_ok;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    // Populate Log (0x0404, CXL 4.0 Table 8-271): 16-byte UUID input.
    // Synchronous model profile; the callback owns the content and must make
    // it available to the subsequent Get Log (0x0401).
    bool populate_log(const std::vector<std::uint8_t>& in, std::uint16_t* rc)
    {
        if (in.size() != kUuidBytes) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        LogEntry* e = find(in);
        if (!e) {
            ++populate_invalid;
            *rc = MBX_RC_INVALID_LOG;
            return true;
        }
        if (!(e->flags & kLogCapPopulateSupported) || !e->populate) {
            ++populate_invalid;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        e->populate();
        ++populate_ok;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

private:
    LogEntry* find(const std::vector<std::uint8_t>& uuid)
    {
        for (auto& e : logs)
            if (uuid.size() == kUuidBytes &&
                std::equal(e.uuid.begin(), e.uuid.end(), uuid.begin()))
                return &e;
        return nullptr;
    }
    const LogEntry* find(const std::vector<std::uint8_t>& uuid) const
    {
        for (const auto& e : logs)
            if (uuid.size() == kUuidBytes &&
                std::equal(e.uuid.begin(), e.uuid.end(), uuid.begin()))
                return &e;
        return nullptr;
    }
};

} // namespace cxl_tlm
