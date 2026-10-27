// -----------------------------------------------------------------------------
// cxl_cache.h -- CXL.cache Type 2 minimal coherent cache (P1).
//
//   Wire format: CXL 4.0 r1.0 Section 3.2.3 / Tables 3-13..3-21.
//   Channel widths come from CxlCoreCfg (68B / 256B HBR / PBR); no hard-coded
//   message widths.
//
//   Implemented model semantics (single Type 2 device + single home agent,
//   64 B lines):
//     * device read miss -> D2H Req (RdShared/RdOwn) -> H2D Data + H2D Rsp GO
//     * write hit M/E stays M; write on I/S upgrades with RdOwn first
//     * dirty evict -> D2H Req DirtyEvict -> H2D Rsp WritePull -> D2H Data
//     * host snoop SnpInv/SnpData/SnpCur -> D2H Rsp (+ D2H Data when dirty)
//     * home reads/writes snoop the device first and absorb dirty data
//
//   Not implemented here (explicit ledger): multi-device snoop filter/directory,
//   HDM-DB/BISnp flows (see cxl_emd_bi), TEE, and out-of-order UQID/CQID
//   tracking beyond the single outstanding request used by this model.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_core_cfg.h"
#include "cxl_msg.h"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace cxl_tlm {

enum class CxlCacheChan : std::uint8_t {
    H2D_REQ = 0, H2D_RSP, H2D_DATA,
    D2H_REQ, D2H_RSP, D2H_DATA,
    COUNT
};

const char* cxl_cache_chan_name(CxlCacheChan c);

enum class CxlCacheState : std::uint8_t { I = 0x3, S = 0x1, E = 0x2, M = 0x6, ERR = 0x4 };
const char* cxl_cache_state_name(CxlCacheState s);

// Table 3-22 (5-bit opcodes).
enum class CxlCacheReq : std::uint8_t {
    RdCurr = 0x01, RdOwn = 0x02, RdShared = 0x03, RdAny = 0x04,
    RdOwnNoData = 0x05, ItoMWr = 0x06, WrCur = 0x07, CLFlush = 0x08,
    CleanEvict = 0x09, DirtyEvict = 0x0A, CleanEvictNoData = 0x0B,
    WOWrInv = 0x0C, WOWrInvF = 0x0D, WrInv = 0x0E, CacheFlushed = 0x10
};

// Table 3-25 (5-bit opcodes).
enum class CxlCacheRsp : std::uint8_t {
    RspSHitSE = 0x01, RspIHitI = 0x04, RspIHitSE = 0x05,
    RspVHitV = 0x06, RspSFwdM = 0x07, RspIFwdM = 0x0F, RspVFwdV = 0x16
};

// Table 3-26 (3-bit opcodes).
enum class CxlCacheSnoop : std::uint8_t { SnpData = 0x1, SnpInv = 0x2, SnpCur = 0x3 };

// Table 3-27 (4-bit opcodes).
enum class CxlCacheH2DRsp : std::uint8_t {
    WritePull = 0x1, GO = 0x4, GO_WritePull = 0x5, ExtCmp = 0x6,
    GO_WritePull_Drop = 0x8, Fast_GO_WritePull = 0xD, GO_ERR_WritePull = 0xF
};

struct CxlCacheFields {
    bool valid = true;
    std::uint8_t opcode = 0;
    std::uint16_t cqid = 0, uqid = 0;
    std::uint16_t spid = 0, dpid = 0;
    std::uint64_t addr = 0;          // 64B aligned; wire carries addr[51:6]
    bool nt = false;
    std::uint8_t cache_id = 0;
    bool chunk_valid = false, bogus = false, poison = false, bep = false, go_err = false;
    std::uint16_t rsp_data = 0;
    std::uint8_t rsp_pre = 0;
    std::array<std::uint8_t, 64> data{};
    std::uint64_t be = ~0ull;
};

int cxl_cache_bits(CxlCacheChan ch, CxlFlitMode mode);   // -1 if unavailable

bool cxl_cache_encode(CxlCacheChan ch, CxlFlitMode mode, const CxlCacheFields& f,
                      CxlPacked* out, std::string* err = nullptr);
bool cxl_cache_decode(CxlCacheChan ch, CxlFlitMode mode, const CxlPacked& in,
                      CxlCacheFields* f, std::string* err = nullptr);
std::string cxl_cache_msg_string(CxlCacheChan ch, const CxlCacheFields& f);

// ------------------------------------------------------------------- home
struct CxlCacheHome {
    std::map<std::uint64_t, std::array<std::uint8_t, 64>> mem;
    std::map<std::uint64_t, CxlCacheState> state;    // host-side line state
    std::uint64_t reads = 0, writes = 0, snoops = 0, writebacks = 0;
    std::uint64_t wire_msgs = 0, wire_errors = 0;

    void preload(std::uint64_t addr, const void* data);
    bool read_mem(std::uint64_t addr, void* out) const;
    bool write_mem(std::uint64_t addr, const void* data);
};

struct CxlCacheDevice;

struct CxlCacheSystem {
    CxlFlitMode mode = CxlFlitMode::B68;
    CxlCacheHome home;
    struct Line {
        bool valid = false;
        CxlCacheState state = CxlCacheState::I;
        std::array<std::uint8_t, 64> data{};
    };
    std::map<std::uint64_t, Line> lines;

    // Statistics (never silent).
    std::uint64_t cache_hits = 0, cache_misses = 0, evictions = 0;
    std::uint64_t snoop_hits = 0, snoop_misses = 0, dirty_writebacks = 0;
    std::uint64_t req_sent = 0, rsp_sent = 0, data_sent = 0, data_rcvd = 0;
    std::uint64_t wire_msgs = 0, wire_errors = 0, protocol_errors = 0;

    // Fault injection for the red self-check (default off).
    bool fault_skip_writeback = false;
    bool fault_wrong_mesi = false;

    bool init(CxlFlitMode m = CxlFlitMode::B68, std::string* err = nullptr);

    // Device-side operations.
    bool cpu_read(std::uint64_t addr, void* out, std::string* err = nullptr);
    bool cpu_read_own(std::uint64_t addr, void* out, std::string* err = nullptr);
    bool cpu_write(std::uint64_t addr, const void* data, std::string* err = nullptr);
    bool cpu_evict(std::uint64_t addr, std::string* err = nullptr);

    // Host-side operations (home agent snoops the device first).
    bool host_read(std::uint64_t addr, void* out, std::string* err = nullptr);
    bool host_write(std::uint64_t addr, const void* data, std::string* err = nullptr);

    // Observability.
    CxlCacheState state_of(std::uint64_t addr) const;
    bool line_copy(std::uint64_t addr, void* out) const;

private:
    bool wire(CxlCacheChan ch, CxlCacheFields in, CxlCacheFields* out, std::string* err);
    bool request(std::uint64_t addr, CxlCacheReq op, CxlCacheFields* go,
                 CxlCacheFields* rsp, std::string* err);
    bool snoop_device(std::uint64_t addr, CxlCacheSnoop snp, std::string* err);
    bool evict_line(std::uint64_t addr, bool dirty, std::string* err);
};

// ------------------------------------------------------- multi-device cache
// P3.1: multiple Type 2 devices + a home directory (sharers + dirty owner).
// Snoops use the real H2D.Req / D2H.Rsp / D2H.Data wire encoders.
struct CxlCacheMultiSystem {
    struct DevLine {
        bool valid = false;
        CxlCacheState state = CxlCacheState::I;
        std::array<std::uint8_t, 64> data{};
        std::uint64_t last_used = 0;               // per-device LRU stamp
    };
    struct Dev {
        std::map<std::uint64_t, DevLine> lines;
        std::uint64_t clock = 0;
        std::uint64_t hits = 0, misses = 0, snoops = 0, evictions = 0;
    };

    CxlFlitMode mode = CxlFlitMode::B68;
    CxlCacheHome home;
    std::vector<Dev> dev;
    // Level-2 home directory: one entry per cached line with all sharers and
    // an optional dirty owner.
    std::map<std::uint64_t, std::set<int>> sharers;
    std::map<std::uint64_t, int> dirty_owner;      // absent = no dirty owner
    std::map<std::uint64_t, std::uint64_t> dir_last_used;
    std::uint64_t dir_clock = 0;

    std::uint64_t snoops = 0, invalidations = 0, writebacks = 0;
    std::uint64_t wire_msgs = 0, wire_errors = 0, protocol_errors = 0;
    // Level-2 directory replacement.  Off by default so a full directory
    // rejects new lines (P4.8 signature); when enabled, a clean LRU victim is
    // invalidated first (dirty victims are written back if no clean line).
    bool lru_replace = false;
    std::size_t max_directory_lines = 4096;
    std::uint64_t directory_overflow = 0;
    std::uint64_t directory_evictions = 0;
    std::uint64_t lru_directory_evictions = 0;
    // Level-1 per-device cache capacity (0 = unlimited).  On overflow the
    // least-recently-used device line is evicted (dirty lines write back).
    std::size_t max_device_lines = 0;
    std::uint64_t lru_device_evictions = 0;

    // Red hooks.
    bool fault_skip_snoop = false;
    bool fault_skip_writeback = false;

    bool init(int n, CxlFlitMode m = CxlFlitMode::B68, std::string* err = nullptr);

    bool dev_read(int id, std::uint64_t addr, void* out, std::string* err = nullptr);
    bool dev_read_own(int id, std::uint64_t addr, void* out, std::string* err = nullptr);
    bool dev_write(int id, std::uint64_t addr, const void* data, std::string* err = nullptr);
    bool dev_evict(int id, std::uint64_t addr, std::string* err = nullptr);

    bool host_read(std::uint64_t addr, void* out, std::string* err = nullptr);
    bool host_write(std::uint64_t addr, const void* data, std::string* err = nullptr);

    CxlCacheState state_of(int id, std::uint64_t addr) const;
    bool line_copy(int id, std::uint64_t addr, void* out) const;
    int owner_of(std::uint64_t addr) const;      // -1 if none
    std::size_t sharer_count(std::uint64_t addr) const;

    // Multi-outstanding UQID/CQID path (P4.6).  Completions may be reordered;
    // duplicate/unknown CQIDs are explicit errors.
    struct Pending {
        int dev = -1;
        std::uint16_t cqid = 0;
        bool write = false;
        std::uint64_t addr = 0;
        std::array<std::uint8_t, 64> data{};
    };
    std::vector<Pending> pending;
    std::uint64_t outstanding_submitted = 0, outstanding_completed = 0;
    std::uint64_t ooo_completions = 0, duplicate_cqid = 0, unknown_cqid = 0;

    bool submit_read(int id, std::uint64_t addr, std::uint16_t cqid,
                     std::string* err = nullptr);
    bool submit_write(int id, std::uint64_t addr, const void* data,
                      std::uint16_t cqid, std::string* err = nullptr);
    bool complete(int id, std::uint16_t cqid, void* out = nullptr,
                  std::string* err = nullptr);
    bool pending_cqid(int id, std::uint16_t cqid) const;

private:
    bool wire(CxlCacheChan ch, CxlCacheFields in, CxlCacheFields* out, std::string* err);
    bool dir_admit(std::uint64_t addr, std::string* err);
    bool snoop_one(int id, std::uint64_t addr, CxlCacheSnoop snp,
                   CxlCacheRsp* rsp, std::array<std::uint8_t, 64>* data,
                   std::string* err);
    void drop_sharer(std::uint64_t addr, int id);
    void touch_dir(std::uint64_t addr);
    bool evict_directory_line(std::uint64_t addr, std::string* err);
    bool reserve_device_line(int id, std::uint64_t addr, std::string* err);
    bool evict_device_line(int id, std::uint64_t addr, std::string* err);
};

} // namespace cxl_tlm
