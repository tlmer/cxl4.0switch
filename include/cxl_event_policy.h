// -----------------------------------------------------------------------------
// cxl_event_policy.h -- Event Interrupt Policy (mailbox 0x0102 / 0x0103).
//
//   CXL 3.0 section 8.2.9.2.4, Table 8-52; Linux 6.18
//   drivers/cxl/cxlmem.h:209-217 and drivers/cxl/pci.c:667-713:
//
//     struct cxl_event_interrupt_policy {
//         u8 info_settings;     // log class 0
//         u8 warn_settings;     // log class 1
//         u8 failure_settings;  // log class 2
//         u8 fatal_settings;    // log class 3
//     } __packed;               // 4 bytes, no padding
//
//   Setting values (Linux enum cxl_event_int_mode):
//     0 = CXL_INT_NONE      (no interrupt; the log is still recorded)
//     1 = CXL_INT_MSI_MSIX  (raise the event MSI/MSI-X vector)
//     2 = CXL_INT_FW        (firmware handles the notification)
//
//   Get (0x0102) has no input payload and returns the four settings.
//   Set (0x0103) takes exactly four bytes and returns no payload.
//   Out-of-range settings are rejected with Invalid Input; wrong payload
//   lengths with Invalid Payload Length.  Nothing is silently clamped.
//
//   This block owns only the policy; the event log itself (append, handle,
//   clear) stays in CxlType3Commands, and the MSI vector is raised by the
//   owner when `interrupt_enabled()` is true.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_mailbox.h"

#include <cstdint>
#include <vector>

namespace cxl_tlm {

enum CxlEventIntMode : std::uint8_t {
    kCxlIntNone     = 0,
    kCxlIntMsiMsix  = 1,
    kCxlIntFw       = 2,
};

struct CxlEventInterruptPolicy {
    static constexpr std::size_t kBytes = 4;

    // Log class order: info (0), warn (1), failure (2), fatal (3).
    std::uint8_t settings[kBytes] = {kCxlIntNone, kCxlIntNone,
                                     kCxlIntNone, kCxlIntNone};

    std::uint64_t get_calls = 0, set_ok = 0, set_rejected = 0;
    std::uint64_t bad_payload = 0, bad_value = 0;

    // CXL 4.0 8.2.10.2.6/8.2.10.2.7 MCTP Event Interrupt Policy.
    // 16-bit bitmask: bits[4:0] log classes, bit15 background-operation
    // completion; bits[14:5] reserved.  Get (0x0104) has no input and
    // returns 2 bytes; Set (0x0105) takes 2 bytes and echoes the stored
    // settings in its 2-byte output.
    std::uint16_t mctp_settings = 0;
    std::uint64_t mctp_get_calls = 0, mctp_set_ok = 0;

    bool valid_mode(std::uint8_t v) const
    {
        return v == kCxlIntNone || v == kCxlIntMsiMsix || v == kCxlIntFw;
    }

    bool interrupt_enabled(int log_class) const
    {
        if (log_class < 0 || log_class >= (int)kBytes) return false;
        return settings[log_class] == kCxlIntMsiMsix;
    }

    bool get(const std::vector<std::uint8_t>& in,
             std::vector<std::uint8_t>* out, std::uint16_t* rc)
    {
        ++get_calls;
        if (!in.empty()) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        out->assign(kBytes, 0);
        for (std::size_t i = 0; i < kBytes; ++i) (*out)[i] = settings[i];
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool set(const std::vector<std::uint8_t>& in, std::uint16_t* rc)
    {
        if (in.size() != kBytes) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        for (std::size_t i = 0; i < kBytes; ++i) {
            if (!valid_mode(in[i])) {
                ++bad_value;
                ++set_rejected;
                *rc = MBX_RC_INVALID_INPUT;
                return true;          // no partial update
            }
        }
        for (std::size_t i = 0; i < kBytes; ++i) settings[i] = in[i];
        ++set_ok;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool mctp_get(const std::vector<std::uint8_t>& in,
                  std::vector<std::uint8_t>* out, std::uint16_t* rc)
    {
        ++mctp_get_calls;
        if (!in.empty()) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        out->assign(2, 0);
        (*out)[0] = (std::uint8_t)(mctp_settings & 0xFF);
        (*out)[1] = (std::uint8_t)(mctp_settings >> 8);
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool mctp_set(const std::vector<std::uint8_t>& in,
                  std::vector<std::uint8_t>* out, std::uint16_t* rc)
    {
        if (in.size() != 2) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        const std::uint16_t v =
            (std::uint16_t)((std::uint16_t)in[0] | ((std::uint16_t)in[1] << 8));
        if ((v & 0x7FE0u) != 0) {              // bits[14:5] reserved
            ++bad_value;
            ++set_rejected;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        mctp_settings = v;
        ++mctp_set_ok;
        out->assign(2, 0);
        (*out)[0] = (std::uint8_t)(mctp_settings & 0xFF);
        (*out)[1] = (std::uint8_t)(mctp_settings >> 8);
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    void clear()
    {
        for (std::size_t i = 0; i < kBytes; ++i) settings[i] = kCxlIntNone;
        mctp_settings = 0;
    }
};

} // namespace cxl_tlm
