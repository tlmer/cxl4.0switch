// -----------------------------------------------------------------------------
// cxl_ide_km.h -- CXL IDE Key Management (IDE_KM) responder over DOE.
//
//   The CXL IDF (IDE Key Management) protocol runs as a DOE object with
//   Vendor ID 1E98h and object type 03h.  This responder implements the model
//   subset needed to drive the IDE key/stream state:
//
//     Request DW0: opcode[7:0] | stream[11:8] | key slot[15:12] |
//                  key type[23:16] (0 = TX, 1 = RX)
//     SET_KEY payload: DW1..DW8 = 32-byte key
//     Response DW0: opcode[7:0] | status[15:8]
//
//   Opcodes: 01h GET_CAPS (rsp DW1 = capability), 02h SET_KEY,
//            03h SET_GO (enable IDE + bind the stream), 04h GET_STATUS
//            (rsp DW1 = status), 05h SET_STOP (unbind the stream).
//   Status: 0 success, 1 invalid input, 2 unsupported, 3 key not ready.
//
//   This is the key/stream management layer; SPDM authentication and the
//   IDE_KM key derivation remain a security-stack integration (the owner
//   supplies key material and the cipher hook).
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_ide.h"

#include <cstdint>
#include <vector>

namespace cxl_tlm {

struct CxlIdeKm {
    CxlIde* ide = nullptr;
    std::uint64_t requests = 0, errors = 0, set_keys = 0, activations = 0;

    bool process(const std::vector<std::uint32_t>& req,
                 std::vector<std::uint32_t>* rsp) {
        if (!ide || !rsp) return false;
        ++requests;
        rsp->clear();
        if (req.empty()) { ++errors; return false; }
        const std::uint32_t dw0 = req[0];
        const std::uint8_t op = (std::uint8_t)(dw0 & 0xFFu);
        const unsigned stream = (dw0 >> 8) & 0xFu;
        const unsigned slot = (dw0 >> 12) & 0xFu;
        const unsigned key_type = (dw0 >> 16) & 0xFFu;
        std::uint32_t status = 0;
        switch (op) {
        case 0x01u:                                   // GET_CAPS
            rsp->push_back((std::uint32_t)op);
            rsp->push_back(ide->capability());
            return true;
        case 0x02u: {                                 // SET_KEY
            if (slot >= CxlIde::kSlots || stream >= CxlIde::kStreams ||
                key_type > 1u || req.size() < 9) {
                status = 1;
                break;
            }
            std::uint8_t key[32];
            for (unsigned d = 0; d < 8; ++d) {
                const std::uint32_t w = req[1 + d];
                for (int i = 0; i < 4; ++i)
                    key[d * 4 + (unsigned)i] = (std::uint8_t)(w >> (8 * i));
            }
            if (!ide->set_key_half(slot, key_type == 0, key)) status = 1;
            else ++set_keys;
            break;
        }
        case 0x03u: {                                 // SET_GO
            ide->ctl |= 0x1u;
            if (!ide->enable_stream(stream, (int)slot)) status = 3;
            else ++activations;
            break;
        }
        case 0x04u:                                   // GET_STATUS
            rsp->push_back((std::uint32_t)op);
            rsp->push_back(ide->status());
            return true;
        case 0x05u:                                   // SET_STOP
            ide->disable_stream(stream);
            break;
        default:
            status = 2;
            break;
        }
        rsp->push_back((std::uint32_t)op | (status << 8));
        if (status != 0) ++errors;
        return true;
    }
};

} // namespace cxl_tlm
