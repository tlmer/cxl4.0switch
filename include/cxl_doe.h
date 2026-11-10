// -----------------------------------------------------------------------------
// cxl_doe.h -- device-side PCIe DOE (Data Object Exchange) responder.
//
//   Completes the model's DOE path: the host helper in cxl_host_doe.{h,cpp}
//   already performs Discovery and CDAT table access; this block answers the
//   standard DOE mailbox registers and the CXL CDAT table-access protocol.
//
//   Register map (relative to the DOE capability, CXL 3.0 8.1.11 / PCIe 6.0):
//     +04 Capability       RO  number of DWs
//     +08 Control          RW  bit0 DOE Enable, bit1 Abort
//     +0C Status           RO  bit0 Busy, bit1 Interrupt, bit2 Error
//     +10 Write Data       WO  request DWs
//     +14 Read Data        RO  response DWs
//
//   Supported objects: DOE Discovery (vendor 0x0001, type 0x01), CMA
//   (vendor 0x0001, type 0x01 with an SPDM payload) and Secured CMA
//   (vendor 0x0001, type 0x02 with an SPDM payload), CXL CDAT table access
//   (vendor 0x1E98, protocol 0x02), and optional IDE_KM.  Other objects are
//   rejected as unsupported, with an error status.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_ide_km.h"
#include "cxl_spdm.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlDoeDevice {
    CxlIdeKm* ide_km = nullptr;      // optional IDE_KM (vendor 1E98h, type 03h)
    CxlSpdmSession* spdm = nullptr;  // optional SPDM session (vendor 0001h, type 01h)
    static constexpr std::uint32_t kCapability = 0x00000100u; // 256 DWs
    static constexpr std::uint16_t kVendorCxl = 0x1E98;

    // SPDM and the legacy DOE Discovery object share (vendor 0001h, type 01h)
    // in this model.  A payload whose first byte is a protocol version
    // (0x10..0x1F) is SPDM; otherwise it keeps the historical Discovery
    // behaviour used by the existing host helper.
    static bool looks_like_spdm(const std::vector<std::uint32_t>& req) {
        return req.size() >= 2 && (req[1] & 0xFFu) >= 0x10u &&
               (req[1] & 0xFFu) <= 0x1Fu;
    }

    std::vector<std::uint8_t> cdat;
    std::vector<std::uint32_t> request;
    std::vector<std::uint32_t> response;
    std::uint32_t ctl = 0;
    std::uint32_t status = 0;
    std::uint16_t cdat_next_handle = 0xFFFFu;
    std::uint64_t discovery_objects = 0, cdat_objects = 0, unsupported = 0;
    std::uint64_t cma_objects = 0, secured_cma_objects = 0;

    void set_cdat(std::vector<std::uint8_t> table) { cdat = std::move(table); }

    std::uint32_t read(std::uint16_t off) const {
        switch (off & ~0x3u) {
        case 0x04: return kCapability;
        case 0x08: return ctl;
        case 0x0C: return status;
        case 0x14: return read_data();
        default: return 0;
        }
    }

    bool write(std::uint16_t off, std::uint32_t v) {
        switch (off & ~0x3u) {
        case 0x08:
            ctl = v;
            if (v & 0x2u) {                 // Abort
                request.clear();
                response.clear();
                status &= ~0x1u;
                cdat_next_handle = 0xFFFFu;
                return true;
            }
            if (v & 0x1u) process();
            return true;
        case 0x10:
            request.push_back(v);
            return true;
        default:
            return false;
        }
    }

private:
    mutable std::size_t rd_pos = 0;

    static void put32(std::vector<std::uint8_t>* v, std::uint32_t x) {
        for (unsigned i = 0; i < 4; ++i)
            v->push_back((std::uint8_t)(x >> (8 * i)));
    }

    static std::uint32_t get32(const std::vector<std::uint8_t>& v,
                               std::size_t off) {
        std::uint32_t x = 0;
        for (unsigned i = 0; i < 4 && off + i < v.size(); ++i)
            x |= (std::uint32_t)v[off + i] << (8 * i);
        return x;
    }

    std::uint32_t read_data() const {
        if (rd_pos >= response.size()) return 0;
        return response[rd_pos++];
    }

    void process() {
        status &= ~0x5u;                    // clear Busy/Error
        response.clear();
        rd_pos = 0;
        if (request.size() < 2) {
            status |= 0x4u;                 // malformed: no header/payload
            return;
        }
        const std::uint32_t hdr = request[0];
        const std::uint16_t vendor = (std::uint16_t)(hdr & 0xFFFFu);
        const std::uint8_t type = (std::uint8_t)((hdr >> 16) & 0xFFu);
        if (vendor == 0x0001u && (type == 0x01u || type == 0x02u) &&
            spdm && spdm->handler && looks_like_spdm(request)) {
            std::vector<std::uint32_t> req(request.begin() + 1, request.end());
            std::vector<std::uint32_t> rsp;
            if (spdm->handler(req, &rsp)) {
                if (type == 0x02u) ++secured_cma_objects;
                else               ++cma_objects;
                const std::uint32_t n = (std::uint32_t)(rsp.size() + 1);
                response.push_back(0x0001u | ((std::uint32_t)type << 16) |
                                   (n << 24));
                for (std::uint32_t w : rsp) response.push_back(w);
                request.clear();
                return;
            }
            ++unsupported;
            status |= 0x4u;
            request.clear();
            return;
        }
        if (vendor == 0x0001u && type == 0x01u) {
            ++discovery_objects;
            response.push_back(0x0001u | (0x01u << 16) | (2u << 24));
            response.push_back(0x00000000u);
            request.clear();
            return;
        }
        if (vendor == kVendorCxl && type == 0x02u) {
            ++cdat_objects;
            const std::uint16_t handle = (std::uint16_t)(request[1] >> 16);
            build_cdat_response(handle);
            request.clear();
            return;
        }
        if (vendor == kVendorCxl && type == 0x03u && ide_km) {
            // IDE_KM object: request[0] is the object header, request[1..]
            // are the IDE_KM request DWs.
            std::vector<std::uint32_t> req(request.begin() + 1, request.end());
            std::vector<std::uint32_t> rsp;
            (void)ide_km->process(req, &rsp);
            const std::uint32_t n = (std::uint32_t)(rsp.size() + 1);
            response.push_back(kVendorCxl | (0x03u << 16) | (n << 24));
            for (std::uint32_t w : rsp) response.push_back(w);
            request.clear();
            return;
        }
        ++unsupported;
        status |= 0x4u;
        request.clear();
    }

    void build_cdat_response(std::uint16_t handle) {
        if (cdat.size() < 16) {
            status |= 0x4u;
            return;
        }
        std::vector<std::uint8_t> payload;
        std::uint16_t next = 0xFFFFu;
        if (handle == 0) {
            // CDAT header (16 bytes); next handle = 1 when entries exist.
            payload.insert(payload.end(), cdat.begin(), cdat.begin() + 16);
            next = (cdat.size() > 16) ? 1u : 0xFFFFu;
        } else {
            // Entry handle 1..N: walk the CDAT entry list.
            std::size_t off = 16;
            std::uint16_t h = 1;
            while (off + 4 <= cdat.size()) {
                const unsigned len = (unsigned)cdat[off + 2] |
                                     ((unsigned)cdat[off + 3] << 8);
                if (len < 4 || off + len > cdat.size()) break;
                if (h == handle) {
                    payload.insert(payload.end(), cdat.begin() + (std::ptrdiff_t)off,
                                   cdat.begin() + (std::ptrdiff_t)(off + len));
                    next = (off + len < cdat.size()) ? (std::uint16_t)(h + 1) : 0xFFFFu;
                    break;
                }
                off += len;
                ++h;
            }
            if (payload.empty()) {
                status |= 0x4u;
                return;
            }
        }
        while ((payload.size() & 3u) != 0) payload.push_back(0);
        // Response payload starts with the CDAT response header (next handle).
        response.push_back((std::uint32_t)next << 16);
        for (std::size_t i = 0; i < payload.size(); i += 4)
            response.push_back(get32(payload, i));
        // DOE response header: +1 for payload DWs, include this header DW.
        const std::uint32_t n = (std::uint32_t)(response.size() + 1);
        response.insert(response.begin(), kVendorCxl | (0x02u << 16) | (n << 24));
        cdat_next_handle = next;
    }
};

} // namespace cxl_tlm
