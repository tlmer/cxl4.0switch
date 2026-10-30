// -----------------------------------------------------------------------------
// cxl_uio.h -- P3.4 UIO Direct P2P to G-FAM + GMV/VTV + GIM.
//
//   GMV (G-FAM Membership Vector): which G-FAM DPIDs a source host PID may
//   access.  VTV (Virtual Translation Vector): which targets a (vCS, LD) pair
//   may access.  FAST segments map HPA ranges to G-FAM DPIDs for UIO Direct
//   P2P.  GIM (Global Identity Memory) is a separately addressed shared memory
//   region; accesses are checked against GMV/VTV and bounds.
//
//   This is a model-level functional view; full DRT/RGT fabric routing is in
//   cxl_fabric.  Unauthorized or unroutable accesses are explicit errors.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_fabric.h"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlUioFastSeg {
    std::uint64_t base = 0, limit = 0;
    std::uint64_t gran = 256;
    std::uint16_t dpid = 0;              // single-target form
    std::vector<std::uint16_t> dpids;    // interleaved targets
    std::uint16_t version = 1;
    bool valid = false;
};

struct CxlUioGim {
    std::uint16_t gim_id = 0;
    std::uint64_t size = 1ull << 20;
    std::map<std::uint64_t, std::array<std::uint8_t, 64>> mem;
    std::uint64_t reads = 0, writes = 0;
};

struct CxlUioGimGroup {
    std::uint16_t group_id = 0;
    bool valid = false;
    std::uint8_t ways = 1;
    std::uint64_t gran = 256;
    std::uint64_t size = 0;
    std::vector<std::uint16_t> targets;
    std::uint64_t reads = 0, writes = 0;
};

// F4: configurable GIM internal address translation / GFD mapping.
// A GIM window (gim_id + [base, base+size)) is translated to a target GIM
// region starting at target_base; gfd is the implementation-specific GFD
// selector carried with the translated access.  Without a mapping entry the
// historical GIM ID + offset behavior is used (identity).
struct CxlUioGimMap {
    std::uint16_t gim_id = 0;
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::uint16_t target = 0;
    std::uint64_t target_base = 0;
    std::uint16_t gfd = 0;
    bool valid = false;
};

struct CxlUioSystem {
    CxlPbrFabric* fabric = nullptr;
    bool pbr_mode = true;

    std::map<std::uint16_t, std::set<std::uint16_t>> gmv;      // PID -> DPIDs
    std::map<std::uint16_t, std::uint16_t> gmv_version;
    std::map<std::pair<std::uint8_t, std::uint8_t>,
             std::set<std::uint16_t>> vtv;                      // (vCS,LD) -> DPIDs
    std::map<std::pair<std::uint8_t, std::uint8_t>, std::uint16_t> vtv_version;
    std::vector<CxlUioFastSeg> fast;
    std::map<std::uint16_t, CxlUioGim> gim;
    std::map<std::uint16_t, CxlUioGimGroup> gim_groups;
    std::vector<CxlUioGimMap> gim_map;              // F4
    // F5: full 4-Kb GMV/VTV register tables (512 bytes per VCS / per VCS+LD).
    std::map<std::uint8_t, std::vector<std::uint8_t>> gmv_table;
    std::map<std::pair<std::uint8_t, std::uint8_t>,
             std::vector<std::uint8_t>> vtv_table;
    std::map<std::pair<std::uint16_t, std::uint16_t>, std::uint16_t> gmv_flags;
    std::map<std::pair<std::uint8_t, std::pair<std::uint8_t, std::uint16_t>>,
             std::uint16_t> vtv_flags;

    std::uint64_t uio_reads = 0, uio_writes = 0;
    std::uint64_t gim_reads = 0, gim_writes = 0;
    std::uint64_t denied_gmv = 0, denied_vtv = 0, unsupported = 0;
    std::uint64_t fast_misses = 0, bounds_errors = 0, fabric_errors = 0;
    std::uint64_t map_misses = 0, map_errors = 0, table_errors = 0;

    void init(CxlPbrFabric* f = nullptr, bool pbr = true);
    bool add_gmv(std::uint16_t pid, std::uint16_t dpid, std::string* err = nullptr);
    bool add_vtv(std::uint8_t vcs_id, std::uint8_t ld_id, std::uint16_t dpid,
                 std::string* err = nullptr);
    bool add_fast(std::uint64_t base, std::uint64_t limit, std::uint64_t gran,
                  std::uint16_t dpid, std::string* err = nullptr);
    bool add_fast_multi(std::uint64_t base, std::uint64_t limit,
                        std::uint64_t gran, const std::vector<std::uint16_t>& dpids,
                        std::uint16_t version = 1, std::string* err = nullptr);
    bool add_gim(std::uint16_t gim_id, std::uint64_t size, std::string* err = nullptr);
    bool add_gim_group(std::uint16_t group_id, std::uint64_t size,
                       std::uint8_t ways, std::uint64_t gran,
                       const std::vector<std::uint16_t>& targets,
                       std::string* err = nullptr);
    // F4: add a GIM internal translation window.
    bool add_gim_map(std::uint16_t gim_id, std::uint64_t base, std::uint64_t size,
                     std::uint16_t target, std::uint64_t target_base,
                     std::uint16_t gfd = 0, std::string* err = nullptr);
    // F4: translate an access through the mapping table (identity if no map).
    bool gim_translate(std::uint16_t gim_id, std::uint64_t off,
                       std::uint16_t* target_gim, std::uint64_t* target_off,
                       std::uint16_t* gfd, std::string* err = nullptr) const;

    // F5: full 4-Kb GMV/VTV register-table access.
    bool set_gmv_bit(std::uint8_t vcs_id, std::uint16_t pid, bool enable,
                     std::string* err = nullptr);
    bool set_vtv_bit(std::uint8_t vcs_id, std::uint8_t ld_id,
                     std::uint16_t dpid, bool enable,
                     std::string* err = nullptr);
    bool gmv_bit(std::uint8_t vcs_id, std::uint16_t pid) const;
    bool vtv_bit(std::uint8_t vcs_id, std::uint8_t ld_id,
                 std::uint16_t dpid) const;
    bool read_gmv_table(std::uint8_t vcs_id, std::uint64_t start,
                        std::uint64_t len, std::vector<std::uint8_t>* out,
                        std::string* err = nullptr) const;
    bool read_vtv_table(std::uint8_t vcs_id, std::uint8_t ld_id,
                        std::uint64_t start, std::uint64_t len,
                        std::vector<std::uint8_t>* out,
                        std::string* err = nullptr) const;
    bool write_gmv_table(std::uint8_t vcs_id, std::uint64_t start,
                         const std::vector<std::uint8_t>& data,
                         std::string* err = nullptr);
    bool write_vtv_table(std::uint8_t vcs_id, std::uint8_t ld_id,
                         std::uint64_t start,
                         const std::vector<std::uint8_t>& data,
                         std::string* err = nullptr);
    void set_gmv_flags(std::uint16_t pid, std::uint16_t dpid,
                       std::uint16_t flags);
    void set_vtv_flags(std::uint8_t vcs_id, std::uint8_t ld_id,
                       std::uint16_t dpid, std::uint16_t flags);
    std::uint16_t gmv_flags_of(std::uint16_t pid, std::uint16_t dpid) const;
    std::uint16_t vtv_flags_of(std::uint8_t vcs_id, std::uint8_t ld_id,
                               std::uint16_t dpid) const;
    void set_gmv_version(std::uint16_t pid, std::uint16_t version);
    void set_vtv_version(std::uint8_t vcs_id, std::uint8_t ld_id,
                         std::uint16_t version);
    std::uint16_t gmv_version_of(std::uint16_t pid) const;
    std::uint16_t vtv_version_of(std::uint8_t vcs_id, std::uint8_t ld_id) const;

    bool fast_lookup(std::uint64_t hpa, std::uint16_t* dpid) const;
    bool gmv_allows(std::uint16_t pid, std::uint16_t dpid) const;
    bool vtv_allows(std::uint8_t vcs_id, std::uint8_t ld_id,
                    std::uint16_t dpid) const;

    // UIO Direct P2P CXL.mem to a G-FAM DPID (FAST must map HPA).
    bool uio_write(std::uint16_t src_pid, std::uint16_t dpid, std::uint64_t hpa,
                   const void* data, std::size_t n, std::string* err = nullptr);
    bool uio_read(std::uint16_t src_pid, std::uint16_t dpid, std::uint64_t hpa,
                  void* data, std::size_t n, std::string* err = nullptr);

    // GIM access with GMV/VTV checks.
    bool gim_write(std::uint16_t host_pid, std::uint8_t vcs_id, std::uint8_t ld_id,
                   std::uint16_t gim_id, std::uint64_t off, const void* data,
                   std::size_t n, std::string* err = nullptr);
    bool gim_read(std::uint16_t host_pid, std::uint8_t vcs_id, std::uint8_t ld_id,
                  std::uint16_t gim_id, std::uint64_t off, void* data,
                  std::size_t n, std::string* err = nullptr);

    // Interleaved GIM group access (group_id is authorized via GMV/VTV).
    bool gim_group_write(std::uint16_t host_pid, std::uint8_t vcs_id,
                         std::uint8_t ld_id, std::uint16_t group_id,
                         std::uint64_t off, const void* data, std::size_t n,
                         std::string* err = nullptr);
    bool gim_group_read(std::uint16_t host_pid, std::uint8_t vcs_id,
                        std::uint8_t ld_id, std::uint16_t group_id,
                        std::uint64_t off, void* data, std::size_t n,
                        std::string* err = nullptr);
};

} // namespace cxl_tlm
