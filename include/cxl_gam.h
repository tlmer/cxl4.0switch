// -----------------------------------------------------------------------------
// cxl_gam.h -- GAE GAM Buffer and GFD enhanced-event payload models (CXL 4.0).
//
//   CXL 4.0 Section 8.2.10.2.9-.12:
//     * 0107h GFD Enhanced Event Notification (GFD -> FM VDM payload)
//     * 0108h GFD to GAE Enhanced Event Notification (GFD -> GAE GAM VDM)
//     * 0109h Get GAM Buffer        (host mailbox command, GAE only)
//     * 010Ah Set GAM Buffer        (host mailbox command, GAE only)
//
//   CxlGamBuffer models the host-memory circular buffer addressed by Get/Set
//   GAM Buffer.  The owner supplies the entry count (2..256; one entry is
//   32 bytes).  GAM VDM payload logging and the Get/Set command payloads are
//   model-level; the GFD transport is owned by the fabric.
//
//   CxlGfdEvent encodes/decodes the two GFD notification payloads.  These are
//   VDM payload formats, not Type-3 host mailbox commands.
// -----------------------------------------------------------------------------
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlGamBuffer {
    static constexpr std::size_t kEntryBytes = 32;
    static constexpr std::uint16_t kMinEntries = 2;
    static constexpr std::uint16_t kMaxEntries = 256;

    bool enabled = false;
    std::uint16_t capacity = 0;       // entries
    std::uint64_t address = 0;        // GAM buffer address, 32-byte aligned
    bool valid = false;
    bool overflow = false;
    std::uint8_t head = 0;            // next entry the GAE writes
    std::uint8_t tail = 0;            // first entry not yet consumed by host

    std::vector<std::uint8_t> memory; // capacity * 32 bytes
    std::uint64_t sets = 0, rejected = 0;
    mutable std::uint64_t gets = 0;
    std::uint64_t logged = 0, overflows = 0, clears = 0;

    // Configure the circular buffer.  entries must be 2..256.
    bool enable(std::uint16_t entries);
    void reset();

    bool empty() const { return head == tail; }
    bool full() const;

    // 010Ah Set GAM Buffer input payload (8 bytes, little endian):
    //   [0] Valid, [1] Clear Overflow, [4:2] reserved,
    //   [51:5] address, [55:52] reserved, [63:56] tail index.
    bool set(const std::vector<std::uint8_t>& in, std::uint16_t* rc);

    // 0109h Get GAM Buffer response payload (8 bytes, little endian):
    //   [0] Valid, [1] Buffer Overflow, [4:2] reserved,
    //   [51:5] address, [55:52] reserved, [63:56] head index.
    bool get(std::vector<std::uint8_t>* out, std::uint16_t* rc) const;

    // Log one 32-byte GAM VDM payload at the head index.  Returns false when
    // the buffer is disabled, invalid, or full; a full buffer sets Overflow.
    bool log(const std::vector<std::uint8_t>& payload);
    std::vector<std::uint8_t> read_entry(std::uint8_t index) const;
};

struct CxlGfdEvent {
    std::uint16_t pbr_id = 0;
    std::uint16_t event = 0;
    std::uint8_t  notification_type = 0;
    std::uint8_t  trigger = 0;
    std::uint16_t rpid = 0;
    std::uint64_t dpa = 0;
    std::uint64_t hpa = 0;
    std::uint16_t group_id = 0;
    std::uint8_t  memory_opcode = 0;
    std::uint16_t flags = 0;
    std::uint16_t handle = 0;
    std::uint64_t timestamp = 0;
};

// 0107h payload (40 bytes).
std::vector<std::uint8_t> encode_gfd_event_0107(const CxlGfdEvent& e);
bool decode_gfd_event_0107(const std::vector<std::uint8_t>& in,
                           CxlGfdEvent* out, std::string* err);
// 0108h payload (32 bytes).  Flags bit[1] is hardwired to 1.
std::vector<std::uint8_t> encode_gfd_event_0108(const CxlGfdEvent& e);
bool decode_gfd_event_0108(const std::vector<std::uint8_t>& in,
                           CxlGfdEvent* out, std::string* err);

} // namespace cxl_tlm
