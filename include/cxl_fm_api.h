// -----------------------------------------------------------------------------
// cxl_fm_api.h -- CXL Fabric Manager API, DCD Management command set 0x56xx.
//
//   Layouts follow CXL 3.2 section 7.6.7.6 (DCD Management) and the QEMU
//   mailbox-utils implementation (GET_DCD_INFO 0x5600, GET_HOST_DC_REGION_
//   CONFIG 0x5601, SET_DC_REGION_CONFIG 0x5602, GET_DC_REGION_EXTENT_LISTS
//   0x5603, INITIATE_DC_ADD 0x5604, INITIATE_DC_RELEASE 0x5605).
//
//   The command state is the same `CxlDcdState` used by the 0x4800-0x4803
//   mailbox commands, so an FM add/release is visible through the host DCD
//   path as well.  Region counts, extents and tags are model-profile values.
//
//   Model choices (documented, not invented wire bytes):
//     * One host is supported; host_id != 0 -> Invalid Input.
//     * Only the prescriptive selection/removal policy (bit 0) is accepted.
//     * Initiating an add requires the extent to be in the offered list;
//       initiating a release requires it to be assigned.
//     * Records are truncated to the mailbox payload size (never silent:
//       `truncated` is incremented).
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_dcd.h"

#include <cstdint>
#include <vector>

namespace cxl_tlm {

struct CxlFmApi {
    // Per-host DCD state for hosts 1..N-1 (host 0 remains the CxlDcdState
    // view used by the 0x4800-0x4803 host mailbox path).  The device capacity
    // (`dcd->offered`) is shared: an FM add pulls an offered extent into the
    // requesting host's assigned list, and release returns it.
    struct HostDcd {
        std::vector<CxlDcdRegion> regions;
        std::vector<CxlDcdExtent> assigned;
    };

    CxlDcdState* dcd = nullptr;
    std::size_t max_payload = 256;      // mailbox payload bytes
    unsigned host_count = 1;            // 1..8 (model profile)
    std::vector<HostDcd> extra_hosts;   // index host-1, host >= 1

    // Configure the number of hosts (default 1).  Each host sees a copy of the
    // device region configuration; capacity is shared through `dcd->offered`.
    void set_host_count(unsigned n);

    std::uint64_t commands = 0, add_ok = 0, release_ok = 0;
    std::uint64_t invalid = 0, unsupported = 0, truncated = 0;
    std::uint64_t host_add_ok[8] = {}, host_release_ok[8] = {};

    bool handle(std::uint16_t opcode, const std::vector<std::uint8_t>& in,
                std::vector<std::uint8_t>* out, std::uint16_t* rc);

private:
    bool host_ok(unsigned h) const { return h < host_count && h < 8; }
    const std::vector<CxlDcdRegion>& regions_of(unsigned h) const
    {
        return (h == 0) ? dcd->regions : extra_hosts[h - 1].regions;
    }
    std::vector<CxlDcdExtent>& assigned_of(unsigned h)
    {
        return (h == 0) ? dcd->assigned : extra_hosts[h - 1].assigned;
    }
    std::uint32_t all_used_tags() const;
    bool get_dcd_info(std::vector<std::uint8_t>* out, std::uint16_t* rc);
    bool get_region_config(const std::vector<std::uint8_t>& in,
                           std::vector<std::uint8_t>* out, std::uint16_t* rc);
    bool set_region_config(const std::vector<std::uint8_t>& in,
                           std::uint16_t* rc);
    bool get_extent_list(const std::vector<std::uint8_t>& in,
                         std::vector<std::uint8_t>* out, std::uint16_t* rc);
    bool initiate_add(const std::vector<std::uint8_t>& in, std::uint16_t* rc);
    bool initiate_release(const std::vector<std::uint8_t>& in,
                          std::uint16_t* rc);
};

} // namespace cxl_tlm
