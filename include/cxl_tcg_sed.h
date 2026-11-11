// -----------------------------------------------------------------------------
// cxl_tcg_sed.h -- Reference TCG/SED security-protocol handler.
//
//   The CXL Security Send/Receive envelope (cxl_security_mailbox.h) carries
//   SECP/SPSP plus protocol data.  This header supplies a reference handler
//   for TCG protocol 0x01:
//
//     * TCG ComPacket / Packet / Subpacket framing (16/24/12-byte headers,
//       little endian, 4-byte padding);
//     * Level 0 Discovery on Security Receive with SPSP 0;
//     * a reference SED state machine inside a Data Subpacket:
//         GET_STATE, UNLOCK, LOCK, SET_ADMIN_PASSWORD, SET_USER_PASSWORD,
//         REVERT.
//
//   The SED command byte/status layout is model-defined; it is not a full
//   Opal method-UID dispatch implementation.  Framing, ComID/SessionID/Seq
//   echo and discovery descriptors follow the TCG transport shape so an
//   owner can replace the command body with its SSC handler.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_security_mailbox.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace cxl_tlm {

struct CxlTcgSubpacket {
    std::uint16_t kind = 0;
    std::vector<std::uint8_t> data;
};

struct CxlTcgPacket {
    std::uint64_t session_id = 0;
    std::uint32_t seq = 0;
    std::uint16_t reserved = 0;
    std::uint16_t ack_type = 0;
    std::uint32_t ack = 0;
    std::vector<CxlTcgSubpacket> subpackets;
};

struct CxlTcgComPacket {
    std::uint16_t com_id = 0;
    std::uint16_t com_id_ext = 0;
    std::uint32_t outstanding = 0;
    std::uint32_t min_transfer = 0;
    CxlTcgPacket packet;
};

struct CxlTcgSed {
    static constexpr std::uint8_t kSecp = 0x01;
    static constexpr std::uint16_t kDiscoverySpsp = 0x0000;
    static constexpr std::uint16_t kDataKind = 0x0000;
    static constexpr std::uint16_t kCreditControlKind = 0x8001;
    static constexpr std::size_t kPasswordBytes = 32;
    static constexpr std::size_t kComHeader = 16;
    static constexpr std::size_t kPacketHeader = 24;
    static constexpr std::size_t kSubHeader = 12;

    enum class Command : std::uint8_t {
        GetState = 0x01,
        Unlock = 0x02,
        Lock = 0x03,
        SetAdminPassword = 0x04,
        SetUserPassword = 0x05,
        Revert = 0x06
    };

    // Reference SED state.  Factory state is locked with media encryption
    // enabled and no passphrases.  Passphrases are compared in fixed time.
    std::uint16_t base_comid = 0x8001;
    bool locked = true;
    bool media_encryption = true;
    bool admin_set = false;
    bool user_set = false;
    std::array<std::uint8_t, kPasswordBytes> admin_password{};
    std::array<std::uint8_t, kPasswordBytes> user_password{};

    std::uint64_t sends = 0, receives = 0, commands = 0;
    std::uint64_t unlocks = 0, locks = 0, reverts = 0;
    std::uint64_t auth_fail = 0, protocol_errors = 0;
    std::uint64_t malformed = 0, unsupported = 0;
    std::uint64_t discovery_reads = 0;

    // ---- little-endian helpers -------------------------------------------
    static void put16(std::vector<std::uint8_t>& v, std::size_t o, std::uint16_t x) {
        v[o] = (std::uint8_t)(x & 0xFF);
        v[o + 1] = (std::uint8_t)(x >> 8);
    }
    static void put32(std::vector<std::uint8_t>& v, std::size_t o, std::uint32_t x) {
        for (int i = 0; i < 4; ++i)
            v[o + (std::size_t)i] = (std::uint8_t)(x >> (8 * i));
    }
    static void put64(std::vector<std::uint8_t>& v, std::size_t o, std::uint64_t x) {
        for (int i = 0; i < 8; ++i)
            v[o + (std::size_t)i] = (std::uint8_t)(x >> (8 * i));
    }
    static std::uint16_t get16(const std::vector<std::uint8_t>& v, std::size_t o) {
        return (std::uint16_t)(v[o] | ((std::uint16_t)v[o + 1] << 8));
    }
    static std::uint32_t get32(const std::vector<std::uint8_t>& v, std::size_t o) {
        std::uint32_t x = 0;
        for (int i = 0; i < 4; ++i)
            x |= (std::uint32_t)v[o + (std::size_t)i] << (8 * i);
        return x;
    }
    static std::uint64_t get64(const std::vector<std::uint8_t>& v, std::size_t o) {
        std::uint64_t x = 0;
        for (int i = 0; i < 8; ++i)
            x |= (std::uint64_t)v[o + (std::size_t)i] << (8 * i);
        return x;
    }

    // ---- framing ---------------------------------------------------------
    static std::size_t padded(std::size_t n) { return (n + 3) & ~(std::size_t)3; }

    static std::vector<std::uint8_t> encode(const CxlTcgComPacket& cp) {
        std::vector<std::uint8_t> sub;
        for (const auto& sp : cp.packet.subpackets) {
            const std::size_t n = padded(sp.data.size());
            const std::size_t base = sub.size();
            sub.resize(base + kSubHeader + n, 0);
            put16(sub, base + 6, sp.kind);
            put32(sub, base + 8, (std::uint32_t)sp.data.size());
            if (!sp.data.empty())
                std::memcpy(sub.data() + base + kSubHeader, sp.data.data(),
                            sp.data.size());
        }
        std::vector<std::uint8_t> out(kComHeader + kPacketHeader + sub.size(), 0);
        put16(out, 0, cp.com_id);
        put16(out, 2, cp.com_id_ext);
        put32(out, 4, cp.outstanding);
        put32(out, 8, cp.min_transfer);
        put32(out, 12, (std::uint32_t)(kPacketHeader + sub.size()));
        const std::size_t p = kComHeader;
        put64(out, p, cp.packet.session_id);
        put32(out, p + 8, cp.packet.seq);
        put16(out, p + 12, cp.packet.reserved);
        put16(out, p + 14, cp.packet.ack_type);
        put32(out, p + 16, cp.packet.ack);
        put32(out, p + 20, (std::uint32_t)sub.size());
        if (!sub.empty())
            std::memcpy(out.data() + p + kPacketHeader, sub.data(), sub.size());
        return out;
    }

    static bool parse(const std::vector<std::uint8_t>& in, CxlTcgComPacket* cp,
                      std::string* err) {
        if (!cp || in.size() < kComHeader) {
            if (err) *err = "TCG ComPacket shorter than 16 bytes";
            return false;
        }
        cp->com_id = get16(in, 0);
        cp->com_id_ext = get16(in, 2);
        cp->outstanding = get32(in, 4);
        cp->min_transfer = get32(in, 8);
        const std::uint32_t plen = get32(in, 12);
        if (plen < kPacketHeader || (std::size_t)plen > in.size() - kComHeader) {
            if (err) *err = "TCG ComPacket Length is out of range";
            return false;
        }
        const std::size_t p = kComHeader;
        cp->packet.session_id = get64(in, p);
        cp->packet.seq = get32(in, p + 8);
        cp->packet.reserved = get16(in, p + 12);
        cp->packet.ack_type = get16(in, p + 14);
        cp->packet.ack = get32(in, p + 16);
        const std::uint32_t packet_len = get32(in, p + 20);
        if (packet_len > plen - kPacketHeader) {
            if (err) *err = "TCG Packet Length exceeds ComPacket";
            return false;
        }
        cp->packet.subpackets.clear();
        std::size_t pos = p + kPacketHeader;
        const std::size_t end = pos + packet_len;
        while (pos + kSubHeader <= end) {
            CxlTcgSubpacket sp;
            sp.kind = get16(in, pos + 6);
            const std::uint32_t n = get32(in, pos + 8);
            if ((std::size_t)n > end - (pos + kSubHeader)) {
                if (err) *err = "TCG Subpacket Length exceeds Packet";
                return false;
            }
            sp.data.assign(in.begin() + (std::ptrdiff_t)(pos + kSubHeader),
                           in.begin() + (std::ptrdiff_t)(pos + kSubHeader + n));
            cp->packet.subpackets.push_back(std::move(sp));
            const std::size_t advance = kSubHeader + padded(n);
            if (advance > end - pos) {
                if (err) *err = "TCG Subpacket padding exceeds Packet";
                return false;
            }
            pos += advance;
        }
        if (pos != end) {
            if (err) *err = "TCG Packet Length does not match Subpackets";
            return false;
        }
        return true;
    }

    // ---- Level 0 Discovery ----------------------------------------------
    // Reference descriptor set: TCG Storage (0x0001), Opal SSC (0x0002),
    // Single User Mode (0x0003), Locking (0x0006).  The Locking payload is
    // model-defined; the descriptor framing is TCG-shaped.
    std::vector<std::uint8_t> discovery() const {
        const std::uint32_t total = 8 + 4 + 8 + 8 + 16;
        std::vector<std::uint8_t> v(total, 0);
        put32(v, 0, total);
        put32(v, 4, 0);
        std::size_t o = 8;
        put16(v, o, 0x0001); v[o + 2] = 1; v[o + 3] = 0; o += 4;
        put16(v, o, 0x0002); v[o + 2] = 1; v[o + 3] = 4;
        put16(v, o + 4, base_comid);
        put16(v, o + 6, 1);
        o += 8;
        put16(v, o, 0x0003); v[o + 2] = 1; v[o + 3] = 4;
        put32(v, o + 4, 1);
        o += 8;
        put16(v, o, 0x0006); v[o + 2] = 1; v[o + 3] = 12;
        v[o + 4] = 0x01;                 // locking supported
        v[o + 5] = 0x01;                 // locking enabled
        v[o + 6] = locked ? 0x01 : 0x00; // currently locked
        v[o + 7] = media_encryption ? 0x01 : 0x00;
        v[o + 8] = 0x00;                 // key protection mechanism
        o += 16;
        return v;
    }

    // ---- reference SED state machine ------------------------------------
    static bool equal_fixed(const std::uint8_t* a, const std::uint8_t* b,
                            std::size_t n) {
        std::uint8_t d = 0;
        for (std::size_t i = 0; i < n; ++i) d |= (std::uint8_t)(a[i] ^ b[i]);
        return d == 0;
    }

    enum Status : std::uint8_t {
        Ok = 0x00, Invalid = 0x01, AuthFail = 0x02, Unsupported = 0x03
    };

    std::vector<std::uint8_t> state_payload(std::uint8_t status) const {
        std::vector<std::uint8_t> v(24, 0);
        v[0] = status;
        v[1] = locked ? 1 : 0;
        v[2] = media_encryption ? 1 : 0;
        v[3] = admin_set ? 1 : 0;
        v[4] = user_set ? 1 : 0;
        put32(v, 8, (std::uint32_t)unlocks);
        put32(v, 12, (std::uint32_t)locks);
        put32(v, 16, (std::uint32_t)reverts);
        put32(v, 20, (std::uint32_t)auth_fail);
        return v;
    }

    std::uint8_t execute(Command cmd, const std::vector<std::uint8_t>& d) {
        switch (cmd) {
        case Command::GetState:
            if (!d.empty()) return Invalid;
            return Ok;
        case Command::Unlock:
            if (d.size() != kPasswordBytes) return Invalid;
            if ((admin_set && equal_fixed(d.data(), admin_password.data(),
                                          kPasswordBytes)) ||
                (user_set && equal_fixed(d.data(), user_password.data(),
                                         kPasswordBytes))) {
                locked = false;
                ++unlocks;
                return Ok;
            }
            ++auth_fail;
            return AuthFail;
        case Command::Lock:
            if (!d.empty()) return Invalid;
            locked = true;
            ++locks;
            return Ok;
        case Command::SetAdminPassword: {
            if (!admin_set) {
                if (d.size() != kPasswordBytes) return Invalid;
                std::memcpy(admin_password.data(), d.data(), kPasswordBytes);
                admin_set = true;
                return Ok;
            }
            if (d.size() != 2 * kPasswordBytes) return Invalid;
            if (!equal_fixed(d.data(), admin_password.data(), kPasswordBytes)) {
                ++auth_fail;
                return AuthFail;
            }
            std::memcpy(admin_password.data(), d.data() + kPasswordBytes,
                        kPasswordBytes);
            return Ok;
        }
        case Command::SetUserPassword: {
            if (!user_set) {
                if (d.size() != kPasswordBytes) return Invalid;
                std::memcpy(user_password.data(), d.data(), kPasswordBytes);
                user_set = true;
                return Ok;
            }
            if (d.size() != 2 * kPasswordBytes) return Invalid;
            if (!equal_fixed(d.data(), user_password.data(), kPasswordBytes)) {
                ++auth_fail;
                return AuthFail;
            }
            std::memcpy(user_password.data(), d.data() + kPasswordBytes,
                        kPasswordBytes);
            return Ok;
        }
        case Command::Revert:
            if (d.size() != kPasswordBytes) return Invalid;
            if (!admin_set ||
                !equal_fixed(d.data(), admin_password.data(), kPasswordBytes)) {
                ++auth_fail;
                return AuthFail;
            }
            locked = true;
            media_encryption = true;
            admin_set = user_set = false;
            admin_password.fill(0);
            user_password.fill(0);
            ++reverts;
            return Ok;
        }
        ++unsupported;
        return Unsupported;
    }

    // ---- envelope handlers ----------------------------------------------
    bool on_send(std::uint8_t secp, std::uint16_t spsp,
                 const std::vector<std::uint8_t>& data,
                 std::vector<std::uint8_t>* response, std::uint16_t* rc) {
        (void)spsp;
        if (secp != kSecp) return false;
        ++sends;
        CxlTcgComPacket cp;
        if (!parse(data, &cp, nullptr)) {
            ++malformed;
            *rc = MBX_RC_INVALID_INPUT;
            return false;
        }
        const CxlTcgSubpacket* body = nullptr;
        for (const auto& sp : cp.packet.subpackets) {
            if (sp.kind == kDataKind) { body = &sp; break; }
        }
        std::uint8_t status = Unsupported;
        std::vector<std::uint8_t> payload;
        if (!body || body->data.empty()) {
            ++protocol_errors;
        } else {
            ++commands;
            const Command cmd = (Command)body->data[0];
            payload.assign(body->data.begin() + 1, body->data.end());
            status = execute(cmd, payload);
        }
        CxlTcgComPacket out;
        out.com_id = cp.com_id ? cp.com_id : base_comid;
        out.com_id_ext = cp.com_id_ext;
        out.packet.session_id = cp.packet.session_id;
        out.packet.seq = cp.packet.seq + 1;
        out.packet.ack_type = cp.packet.ack_type;
        out.packet.ack = cp.packet.ack;
        CxlTcgSubpacket sp;
        sp.kind = kDataKind;
        sp.data = state_payload(status);
        out.packet.subpackets.push_back(std::move(sp));
        *response = encode(out);
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool on_receive(std::uint8_t secp, std::uint16_t spsp,
                    const std::vector<std::uint8_t>& data,
                    std::vector<std::uint8_t>* response, std::uint16_t* rc) {
        if (secp != kSecp) return false;
        if (spsp != kDiscoverySpsp || !data.empty()) {
            *rc = MBX_RC_UNSUPPORTED;
            return false;
        }
        ++receives;
        ++discovery_reads;
        *response = discovery();
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool attach(CxlSecurityMailbox& sm) {
        sm.enabled = true;
        const bool send_ok = sm.register_protocol(
            kSecp, [this](std::uint8_t s, std::uint16_t p,
                          const std::vector<std::uint8_t>& d,
                          std::vector<std::uint8_t>* r, std::uint16_t* rc) {
                return on_send(s, p, d, r, rc);
            });
        const bool recv_ok = sm.register_receive_protocol(
            kSecp, [this](std::uint8_t s, std::uint16_t p,
                          const std::vector<std::uint8_t>& d,
                          std::vector<std::uint8_t>* r, std::uint16_t* rc) {
                return on_receive(s, p, d, r, rc);
            });
        return send_ok && recv_ok;
    }

    void factory_reset() {
        locked = true;
        media_encryption = true;
        admin_set = user_set = false;
        admin_password.fill(0);
        user_password.fill(0);
    }
};

} // namespace cxl_tlm
