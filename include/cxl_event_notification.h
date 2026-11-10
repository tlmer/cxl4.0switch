// -----------------------------------------------------------------------------
// cxl_event_notification.h -- CXL Event Notification payload / queue.
//
//   Opcode 0106h is component-originated on the MCTP CCI; it is not a host
//   mailbox request (a host write to the mailbox is not a notification).  The
//   model exposes the 2-byte payload defined by the Type-3 management ECN:
//     bit 0   informational event log has uncleared records
//     bit 1   warning event log ...
//     bit 2   failure event log ...
//     bit 3   fatal event log ...
//     bit 15  background operation completed
//     bits 4..14 reserved (must be zero)
//
//   The queue/encoder is transport-independent; the owner (MCTP CCI / FM
//   link) consumes messages and performs the 1 ms retry policy.
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <deque>
#include <string>

namespace cxl_tlm {

struct CxlEventNotification {
    static constexpr std::uint16_t kInformational = 1u << 0;
    static constexpr std::uint16_t kWarning       = 1u << 1;
    static constexpr std::uint16_t kFailure       = 1u << 2;
    static constexpr std::uint16_t kFatal         = 1u << 3;
    static constexpr std::uint16_t kBackgroundDone = 1u << 15;
    static constexpr std::uint16_t kReservedMask  = 0x7FF0u;

    static bool valid(std::uint16_t events) {
        return events != 0 && (events & kReservedMask) == 0;
    }
    static std::uint16_t encode(bool informational, bool warning, bool failure,
                                bool fatal, bool background_done) {
        return (std::uint16_t)((informational ? kInformational : 0u) |
                               (warning ? kWarning : 0u) |
                               (failure ? kFailure : 0u) |
                               (fatal ? kFatal : 0u) |
                               (background_done ? kBackgroundDone : 0u));
    }
};

// Bounded queue for the outbound notifications.  `post` rejects an empty or
// reserved-bit payload and a full queue; every path is counted, never silent.
struct CxlEventNotificationQueue {
    std::size_t capacity = 8;
    std::deque<std::uint16_t> pending;
    std::uint64_t posted = 0, consumed = 0, rejected = 0, dropped_full = 0;

    bool post(std::uint16_t events, std::string* err = nullptr) {
        if (!CxlEventNotification::valid(events)) {
            ++rejected;
            if (err) *err = "event notification: empty or reserved bit set";
            return false;
        }
        if (pending.size() >= capacity) {
            ++dropped_full;
            if (err) *err = "event notification queue full";
            return false;
        }
        pending.push_back(events);
        ++posted;
        return true;
    }

    bool pop(std::uint16_t* events, std::string* err = nullptr) {
        if (!events) {
            ++rejected;
            if (err) *err = "event notification: null output";
            return false;
        }
        if (pending.empty()) {
            if (err) *err = "event notification queue empty";
            return false;
        }
        *events = pending.front();
        pending.pop_front();
        ++consumed;
        return true;
    }

    bool has_pending() const { return !pending.empty(); }
};

} // namespace cxl_tlm
