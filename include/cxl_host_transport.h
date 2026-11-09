// -----------------------------------------------------------------------------
// cxl_host_transport.h -- host-side transport used when the Linux proxy drives
// an external DUT (RTL/DPI) instead of the model's local CxlSwitch.
//
//   Configuration traffic is BDF/register based; CXL.mem traffic is HPA based.
//   The local model implementation and the DPI/RTL implementation both satisfy
//   this interface, so the Linux proxy and remote enumerator do not need to
//   know whether the endpoint is local or a DUT.
// -----------------------------------------------------------------------------
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlHostTransport {
    virtual ~CxlHostTransport() = default;

    virtual bool cfg_read(std::uint8_t bus, std::uint8_t dev, std::uint8_t fn,
                          std::uint16_t off, unsigned size, std::uint32_t* val,
                          std::string* err = nullptr) = 0;
    virtual bool cfg_write(std::uint8_t bus, std::uint8_t dev, std::uint8_t fn,
                           std::uint16_t off, unsigned size, std::uint32_t val,
                           std::string* err = nullptr) = 0;

    virtual bool mem_read(std::uint64_t hpa, void* dst, std::size_t n,
                          std::string* err = nullptr) = 0;
    virtual bool mem_write(std::uint64_t hpa, const void* src, std::size_t n,
                           std::string* err = nullptr) = 0;

    // Link/init gate exposed by the DUT transport.  The local model returns
    // true immediately; the DPI transport waits for rdlh_link_up and the
    // CXL link-layer init done.
    virtual bool wait_link(unsigned timeout_ms, std::string* err = nullptr) = 0;

    // Optional root-initiated PCIe Message TLP (type is the 5-bit rrr-encoded
    // type, code is the 8-bit message code, reqid is the root's requester ID).
    // Transports without a message channel return false and say why; the
    // default is never a silent success.
    virtual bool send_message(std::uint8_t type, std::uint16_t code,
                              std::uint16_t reqid,
                              std::string* err = nullptr) {
        (void)type;
        (void)code;
        (void)reqid;
        if (err) *err = "transport does not implement PCIe message TLPs";
        return false;
    }

    // Payload form.  Payload bytes are the message data DWs in wire order
    // (little-endian); an empty payload is identical to send_message().
    // Transports that only carry no-data messages reject non-empty payloads
    // explicitly instead of silently dropping them.
    virtual bool send_message_payload(std::uint8_t type, std::uint16_t code,
                                      std::uint16_t reqid,
                                      const std::vector<std::uint8_t>& payload,
                                      std::string* err = nullptr) {
        if (!payload.empty()) {
            if (err) *err = "transport does not implement PCIe message payloads";
            return false;
        }
        return send_message(type, code, reqid, err);
    }
};

// PCIe message routing uses the 5-bit 10_rrr type field.  r = 000 route to
// root, 010 route by ID, 011 broadcast, 100 local terminate / hop-by-hop,
// 101 gather to root.  Keeping the encoding in one helper avoids the type
// mistakes that a raw 0x10/0x14 constant invites.
inline std::uint8_t cxl_host_msg_type(unsigned r) {
    return static_cast<std::uint8_t>(0x10u | (r & 0x7u));
}

inline bool cxl_host_send_message(CxlHostTransport& tr, unsigned r,
                                  std::uint8_t code, std::uint16_t reqid,
                                  const std::vector<std::uint8_t>& payload = {},
                                  std::string* err = nullptr) {
    return tr.send_message_payload(cxl_host_msg_type(r), code, reqid, payload,
                                   err);
}

// Root-initiated PME_Turn_Off: broadcast (r=011, type 0x13) message 0x19.
// A device answers with PME_TO_Ack (0x1B), which CxlRoot::receive_message
// accounts for.  Returns the transport result.
inline bool cxl_host_send_pme_turn_off(CxlHostTransport& tr,
                                       std::uint16_t reqid,
                                       std::string* err = nullptr) {
    return cxl_host_send_message(tr, 3, 0x19, reqid, {}, err);
}

// PME_TO_Ack: r=101, code 0x1B (gather to root).
inline bool cxl_host_send_pme_to_ack(CxlHostTransport& tr,
                                     std::uint16_t reqid,
                                     std::string* err = nullptr) {
    return cxl_host_send_message(tr, 5, 0x1B, reqid, {}, err);
}

// PM_PME: r=000, code 0x18.
inline bool cxl_host_send_pm_pme(CxlHostTransport& tr, std::uint16_t reqid,
                                 std::string* err = nullptr) {
    return cxl_host_send_message(tr, 0, 0x18, reqid, {}, err);
}

// AER error messages (ERR_COR 0x30 / ERR_NONFATAL 0x31 / ERR_FATAL 0x33),
// all r=000.  The error source/status payload is carried by the message
// header; these helpers send the no-data form.
inline bool cxl_host_send_aer_cor(CxlHostTransport& tr, std::uint16_t reqid,
                                  std::string* err = nullptr) {
    return cxl_host_send_message(tr, 0, 0x30, reqid, {}, err);
}
inline bool cxl_host_send_aer_nonfatal(CxlHostTransport& tr,
                                       std::uint16_t reqid,
                                       std::string* err = nullptr) {
    return cxl_host_send_message(tr, 0, 0x31, reqid, {}, err);
}
inline bool cxl_host_send_aer_fatal(CxlHostTransport& tr, std::uint16_t reqid,
                                    std::string* err = nullptr) {
    return cxl_host_send_message(tr, 0, 0x33, reqid, {}, err);
}

// LTR: r=100, code 0x10, one DW payload (latency values).  The caller owns
// the LTR field encoding; this helper only guarantees the routing type/code.
inline bool cxl_host_send_ltr(CxlHostTransport& tr, std::uint16_t reqid,
                              std::uint32_t latency_values,
                              std::string* err = nullptr) {
    std::vector<std::uint8_t> p(4);
    for (unsigned i = 0; i < 4; ++i)
        p[i] = static_cast<std::uint8_t>(latency_values >> (8 * i));
    return cxl_host_send_message(tr, 4, 0x10, reqid, p, err);
}

// OBFF: r=100, code 0x12, no data.
inline bool cxl_host_send_obff(CxlHostTransport& tr, unsigned r,
                               std::uint16_t reqid,
                               std::string* err = nullptr) {
    return cxl_host_send_message(tr, r, 0x12, reqid, {}, err);
}

// VDM: Type 0 = r=000 / 0x7E, Type 1 = r=010 / 0x7F.  Payload is the VDM
// data as supplied by the caller.
inline bool cxl_host_send_vdm(CxlHostTransport& tr, bool type1,
                              std::uint16_t reqid,
                              const std::vector<std::uint8_t>& payload,
                              std::string* err = nullptr) {
    return cxl_host_send_message(tr, type1 ? 2u : 0u, type1 ? 0x7F : 0x7E,
                                 reqid, payload, err);
}

// PTM Request (0x52) / Response (0x53): r=100, 4-byte payload supplied by
// the caller (PTM message payload layout is caller-owned).
inline bool cxl_host_send_ptm_request(CxlHostTransport& tr,
                                      std::uint16_t reqid,
                                      const std::vector<std::uint8_t>& payload,
                                      std::string* err = nullptr) {
    return cxl_host_send_message(tr, 4, 0x52, reqid, payload, err);
}
inline bool cxl_host_send_ptm_response(CxlHostTransport& tr,
                                       std::uint16_t reqid,
                                       const std::vector<std::uint8_t>& payload,
                                       std::string* err = nullptr) {
    return cxl_host_send_message(tr, 4, 0x53, reqid, payload, err);
}

} // namespace cxl_tlm
