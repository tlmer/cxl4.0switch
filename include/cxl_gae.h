// -----------------------------------------------------------------------------
// cxl_gae.h -- GAE GAM Buffer endpoint and GFD notification source (model).
//
//   CXL 4.0 Section 8.2.10.2.10-.12:
//     * a GFD sends 0108h enhanced-event payloads to a host GAE as GAM VDMs;
//     * the GAE logs each 32-byte payload in the host-memory GAM Buffer;
//     * the host reads/updates the buffer pointer with 0109h/010Ah.
//
//   This is the model-level endpoint composition: CxlGae owns a CxlGamBuffer
//   and a bounded VDM inbox, and exposes the two host mailbox commands.
//   CxlGfdPort encodes a 0108h payload and hands the VDM to the GAE.  The
//   real fabric/VDM transport and host-memory ownership remain platform-side.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_gam.h"
#include "cxl_mailbox.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace cxl_tlm {

struct CxlGae {
    static constexpr std::size_t kMaxInbox = 64;

    CxlGamBuffer gam;
    std::deque<std::vector<std::uint8_t>> inbox;   // pending 32-byte GAM VDMs
    std::uint64_t vdm_received = 0, vdm_delivered = 0, vdm_dropped = 0;
    std::uint64_t unsupported_cmds = 0;

    bool enable(std::uint16_t entries);
    // Host mailbox 0109h/010Ah.  Other opcodes return Unsupported.
    bool host_command(std::uint16_t opcode, const std::vector<std::uint8_t>& in,
                      std::vector<std::uint8_t>* out, std::uint16_t* rc);
    // Enqueue one GAM VDM payload (exactly 32 bytes).  A full inbox drops
    // explicitly and increments vdm_dropped.
    bool receive_gam_vdm(const std::vector<std::uint8_t>& payload);
    // Move the oldest pending VDM into the GAM Buffer.  Returns false when the
    // inbox is empty or the buffer is disabled/invalid/full (the drop is
    // counted by CxlGamBuffer and vdm_dropped).
    bool deliver_one();
    std::size_t pending() const { return inbox.size(); }
};

struct CxlGfdPort {
    CxlGae* gae = nullptr;
    std::uint64_t events_sent = 0, events_rejected = 0;

    // Encode a 0108h payload and hand it to the GAE as a GAM VDM.
    bool send_enhanced_event(const CxlGfdEvent& e);
};

} // namespace cxl_tlm
