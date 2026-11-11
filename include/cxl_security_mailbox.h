// -----------------------------------------------------------------------------
// cxl_security_mailbox.h -- CXL Security Send/Receive envelope (0x4600/0x4601).
//
//   CXL defines the transport envelope, not the security protocol semantics:
//     byte 00     Security Protocol (SECP)
//     bytes 01-02 Security Protocol Specific (SPSP, little endian)
//     bytes 03-07 Reserved
//     bytes 08..  protocol-specific data
//
//   The model keeps the protocol semantics owner-supplied:
//     - disabled by default; 0x4600/0x4601 remain Unsupported;
//     - when enabled, only SECP values with a registered handler are accepted;
//     - a successful Security Send queues its response for the matching
//       Security Receive (SECP+SPSP);
//     - an optional receive-only handler serves protocol-defined unsolicited
//       responses when no Send response is pending (for example TCG Level 0
//       Discovery).  `cxl_tcg_sed.h` supplies a reference TCG/SED handler.
//
//   This is intentionally an envelope/registry, not a full TCG/SED
//   conformance implementation.  Unknown protocols are never silently
//   accepted.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_mailbox.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <vector>

namespace cxl_tlm {

struct CxlSecurityMailbox {
    static constexpr std::size_t kHeaderBytes = 8;
    static constexpr std::size_t kMaxPendingPerKey = 8;

    // handler(secp, spsp, data) -> response.  Return false and set rc to
    // reject the request; leave rc untouched to get Unsupported.
    using Handler = std::function<bool(
        std::uint8_t secp, std::uint16_t spsp,
        const std::vector<std::uint8_t>& data,
        std::vector<std::uint8_t>* response, std::uint16_t* rc)>;

    bool enabled = false;
    std::map<std::uint8_t, Handler> handlers;
    // Optional receive-side handler for protocol-defined unsolicited
    // responses (for example TCG Level 0 Discovery).  It is consulted only
    // when no Send response is pending for the same SECP+SPSP.
    std::map<std::uint8_t, Handler> receive_handlers;
    std::map<std::uint32_t, std::deque<std::vector<std::uint8_t>>> pending;
    std::uint64_t sends = 0, receives = 0, rejected = 0;
    std::uint64_t unsupported_protocol = 0, malformed = 0, empty_receive = 0;

    bool register_protocol(std::uint8_t secp, Handler h) {
        if (!h) return false;
        handlers[secp] = std::move(h);
        return true;
    }

    bool register_receive_protocol(std::uint8_t secp, Handler h) {
        if (!h) return false;
        receive_handlers[secp] = std::move(h);
        return true;
    }

    void clear() {
        pending.clear();
        sends = receives = rejected = 0;
        unsupported_protocol = malformed = empty_receive = 0;
    }

    bool send(const std::vector<std::uint8_t>& in,
              std::vector<std::uint8_t>* out, std::uint16_t* rc) {
        ++sends;
        out->clear();
        if (!enabled) {
            ++rejected;
            *rc = MBX_RC_UNSUPPORTED;
            return false;
        }
        if (in.size() < kHeaderBytes) {
            ++malformed;
            ++rejected;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return false;
        }
        const std::uint8_t secp = in[0];
        const std::uint16_t spsp = (std::uint16_t)(in[1] | (in[2] << 8));
        for (std::size_t i = 3; i < kHeaderBytes; ++i) {
            if (in[i] != 0) {
                ++malformed;
                ++rejected;
                *rc = MBX_RC_INVALID_INPUT;
                return false;
            }
        }
        auto it = handlers.find(secp);
        if (it == handlers.end()) {
            ++unsupported_protocol;
            ++rejected;
            *rc = MBX_RC_UNSUPPORTED;
            return false;
        }
        std::vector<std::uint8_t> data(in.begin() + kHeaderBytes, in.end());
        std::vector<std::uint8_t> response;
        *rc = MBX_RC_SUCCESS;
        if (!it->second(secp, spsp, data, &response, rc)) {
            ++rejected;
            if (*rc == MBX_RC_SUCCESS) *rc = MBX_RC_UNSUPPORTED;
            return false;
        }
        const std::uint32_t key =
            ((std::uint32_t)secp << 16) | (std::uint32_t)spsp;
        auto& q = pending[key];
        if (q.size() >= kMaxPendingPerKey) {
            ++rejected;
            *rc = MBX_RC_RESOURCES_EXHAUSTED;
            return false;
        }
        q.push_back(std::move(response));
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool receive(const std::vector<std::uint8_t>& in,
                 std::vector<std::uint8_t>* out, std::uint16_t* rc) {
        ++receives;
        out->clear();
        if (!enabled) {
            ++rejected;
            *rc = MBX_RC_UNSUPPORTED;
            return false;
        }
        if (in.size() < kHeaderBytes) {
            ++malformed;
            ++rejected;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return false;
        }
        const std::uint8_t secp = in[0];
        const std::uint16_t spsp = (std::uint16_t)(in[1] | (in[2] << 8));
        const std::uint32_t key =
            ((std::uint32_t)secp << 16) | (std::uint32_t)spsp;
        auto it = pending.find(key);
        if (it == pending.end() || it->second.empty()) {
            auto rh = receive_handlers.find(secp);
            if (rh != receive_handlers.end()) {
                std::vector<std::uint8_t> data(in.begin() + kHeaderBytes,
                                               in.end());
                std::vector<std::uint8_t> response;
                *rc = MBX_RC_SUCCESS;
                if (rh->second(secp, spsp, data, &response, rc)) {
                    *out = std::move(response);
                    *rc = MBX_RC_SUCCESS;
                    return true;
                }
                if (*rc == MBX_RC_SUCCESS) *rc = MBX_RC_UNSUPPORTED;
                ++rejected;
                return false;
            }
            ++empty_receive;
            ++rejected;
            *rc = MBX_RC_INVALID_INPUT;
            return false;
        }
        *out = std::move(it->second.front());
        it->second.pop_front();
        if (it->second.empty()) pending.erase(it);
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool has_pending(std::uint8_t secp, std::uint16_t spsp) const {
        const std::uint32_t key =
            ((std::uint32_t)secp << 16) | (std::uint32_t)spsp;
        auto it = pending.find(key);
        return it != pending.end() && !it->second.empty();
    }
};

} // namespace cxl_tlm
