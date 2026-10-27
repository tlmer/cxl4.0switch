// -----------------------------------------------------------------------------
// cxl_fabric.h -- P2 multi-level PBR fabric / ISL / G-FAM model.
//
//   A fabric is a set of PBR switches connected by inter-switch links (ISLs).
//   Each switch has a DPID Routing Table (DRT): DPID -> egress port, plus a
//   local endpoint (G-FAM device).  Routing follows the DRT hop by hop; loops,
//   missing routes, downed links, and hop-limit exhaustion are explicit errors.
//
//   G-FAM: a Type 3 device reachable by DPID; shared mode means all hosts see
//   the same DPA contents (pooled/shared FAM).  The model abstracts the PBR
//   physical link; hop count and link latency are tracked.
// -----------------------------------------------------------------------------
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace cxl_tlm {

constexpr int kFabricLocalPort = 0xFF;
constexpr int kFabricRgtFlag   = 0x100;   // DRT entry: RGT index

struct CxlFabricRgt {
    std::vector<int> ports;
    // 0=static (legacy), 1=round-robin (legacy), 2=random (legacy),
    // 3=least-used CA (legacy), 4=primary/secondary preferred (legacy),
    // 5=Mix with Random, 6=Mix with Congestion Avoidance,
    // 7=Advanced Congestion Avoidance (CXL 4.0 Table 7-87).
    std::uint8_t mode = 0;
    std::uint8_t mix = 0;                 // Table 7-87 mix setting 0..7
    std::uint8_t highest_primary = 0;
    std::uint64_t next = 0;               // round-robin/random/mix state
    std::uint64_t hits = 0;
    mutable std::uint64_t primary_hits = 0, secondary_hits = 0;
    mutable std::uint64_t stalls = 0;     // selected group had no live path
    mutable std::uint64_t port_hits[8] = {0,0,0,0,0,0,0,0};
};

struct CxlFabricGfam {
    std::uint16_t dpid = 0;
    bool shared = true;
    std::uint64_t size = 16ull << 20;
    std::map<std::uint64_t, std::array<std::uint8_t, 64>> mem;
    std::map<std::uint16_t, std::uint64_t> host_accesses;   // SPID -> count
    std::uint64_t reads = 0, writes = 0;
};

struct CxlFabricSwitch {
    int id = 0;
    int ports = 0;
    std::uint16_t pid = 0x0FFF;
    std::map<std::uint16_t, int> drt;      // DPID -> port / LOCAL / RGT flag|index
    mutable std::map<int, CxlFabricRgt> rgt;
    CxlFabricGfam* gfam = nullptr;         // endpoint attached to this switch
    // Vendor congestion metric per egress port (advanced CA input; 0 = idle).
    std::uint64_t port_congestion[8] = {0,0,0,0,0,0,0,0};
    // Fixed cost added to a secondary candidate in advanced CA (prefers
    // minimal-hop primary paths until they become congested).
    std::uint64_t secondary_penalty = 64;
};

struct CxlFabricLink {
    int a = 0, a_port = 0, b = 0, b_port = 0;
    std::uint32_t latency = 1;
    bool up = true;
};

struct CxlPbrFabric {
    std::vector<CxlFabricSwitch> sw;
    std::vector<CxlFabricLink> links;
    std::map<std::uint16_t, CxlFabricGfam> gfam;

    bool pbr_mode = true;                  // 68B profile -> G-FAM/PBR unsupported
    std::uint32_t max_hops = 8;

    mutable std::uint64_t routed = 0, hops = 0, misses = 0, loops = 0;
    mutable std::uint64_t link_down = 0, unsupported = 0, bad_dpid = 0;
    mutable std::uint64_t rgt_hits = 0, rgt_errors = 0;
    mutable std::uint64_t rgt_stalls = 0;
    mutable int last_first_port = -1;
    mutable std::uint64_t rng_state = 0x12345678ull;

    bool init(int n);
    bool connect(int a, int a_port, int b, int b_port, std::uint32_t latency = 1,
                 std::string* err = nullptr);
    bool add_gfam(int sw_id, std::uint16_t dpid, bool shared,
                  std::uint64_t bytes = 16ull << 20, std::string* err = nullptr);
    bool add_route(int sw_id, std::uint16_t dpid, int egress_port,
                   std::string* err = nullptr);
    bool add_rgt(int sw_id, int rgt_index, const std::vector<int>& ports,
                 std::uint8_t mode = 1, std::string* err = nullptr);
    bool add_rgt(int sw_id, int rgt_index, const std::vector<int>& ports,
                 std::uint8_t mode, std::uint8_t mix,
                 std::uint8_t highest_primary, std::string* err);
    bool add_route_rgt(int sw_id, std::uint16_t dpid, int rgt_index,
                       std::string* err = nullptr);
    void set_port_congestion(int sw_id, int port, std::uint64_t metric);

    // Resolve a DPID from (sw, ingress_port).  On success, `egress_sw` is the
    // final switch, `egress_port` is LOCAL for a local endpoint, and `hops_out`
    // is the number of ISL traversals.
    bool route(int sw_id, int ingress_port, std::uint16_t dpid,
               int* egress_sw, int* egress_port, std::uint32_t* hops_out,
               std::string* err = nullptr) const;

    bool gfam_write(int src_sw, std::uint16_t dpid, std::uint64_t dpa,
                    const void* data, std::size_t n, std::string* err = nullptr);
    bool gfam_read(int src_sw, std::uint16_t dpid, std::uint64_t dpa,
                   void* data, std::size_t n, std::string* err = nullptr);
};

} // namespace cxl_tlm
