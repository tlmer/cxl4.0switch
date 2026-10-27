// -----------------------------------------------------------------------------
// cxl_emd_bi.h -- Extended Metadata (EMD) trailer + HDM-DB BISnp/BIRsp (P1).
//
//   EMD: CXL 4.0 Section 3.3.6/4.3.3.  The trailer is appended after the data
//   of an M2S RwD or S2M DRS message.  This model supports the 32-bit EMD
//   trailer in 256B/PBR modes; 68B has no EMD trailer (explicitly rejected).
//
//   BISnp/BIRsp: Section 3.3.7/3.3.8.  Field layouts follow Tables 3-44/3-46;
//   68B flit mode does not support these channels.
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

// --------------------------------------------------------------- EMD trailer
int cxl_emd_trailer_bits(CxlMsgClass cls, CxlFlitMode mode);   // max (32), -1 unavailable

// Variable-width EMD (1..32 bits) and optional 64-bit BE trailer.
struct CxlEmdTrailer {
    bool has_be = false;
    std::uint64_t be = ~0ull;
    bool has_emd = false;
    std::uint32_t emd = 0;
    int emd_bits = 32;
};

int cxl_trailer_bits(CxlMsgClass cls, CxlFlitMode mode, const CxlEmdTrailer& t);
bool cxl_trailer_encode(CxlMsgClass cls, CxlFlitMode mode, const CxlMsgFields& f,
                        const CxlEmdTrailer& t, CxlPacked* out,
                        std::string* err = nullptr);
bool cxl_trailer_decode(CxlMsgClass cls, CxlFlitMode mode, const CxlPacked& in,
                        CxlMsgFields* f, CxlEmdTrailer* t,
                        std::string* err = nullptr);

bool cxl_emd_encode(CxlMsgClass cls, CxlFlitMode mode, const CxlMsgFields& f,
                    std::uint32_t emd, CxlPacked* out, std::string* err = nullptr);
bool cxl_emd_encode(CxlMsgClass cls, CxlFlitMode mode, const CxlMsgFields& f,
                    std::uint32_t emd, int emd_bits, CxlPacked* out,
                    std::string* err = nullptr);
bool cxl_emd_decode(CxlMsgClass cls, CxlFlitMode mode, const CxlPacked& in,
                    CxlMsgFields* f, std::uint32_t* emd, std::string* err = nullptr);
bool cxl_emd_decode(CxlMsgClass cls, CxlFlitMode mode, const CxlPacked& in,
                    CxlMsgFields* f, std::uint32_t* emd, int emd_bits,
                    std::string* err = nullptr);

struct CxlEmdErr {
    bool valid = false;
    std::uint64_t addr = 0;
    std::uint8_t meta_field = 0, meta_value = 0;
    std::uint32_t expected = 0, actual = 0;
    int emd_bits = 32;
};

// Verify the EMD trailer against an expected value; on mismatch fill `err` and
// return false (the caller must report EMDErr -- never silently accept).
bool cxl_emd_verify(const CxlPacked& in, int emd_bits, std::uint32_t expected,
                    std::uint64_t addr, CxlEmdErr* err);

// ------------------------------------------------------------- BI messages
struct CxlBiMsg {
    bool valid = true;
    std::uint8_t opcode = 0;
    std::uint16_t bi_id = 0, bi_tag = 0;
    std::uint64_t addr = 0;          // 64B aligned
    std::uint16_t spid = 0, dpid = 0;
    std::uint8_t low_addr = 0;       // BIRsp LowAddr[1:0]
};

enum : std::uint8_t {
    BISNP_CUR = 0x0, BISNP_DATA = 0x1, BISNP_INV = 0x2,
    BISNP_CUR_BLK = 0x4, BISNP_DATA_BLK = 0x5, BISNP_INV_BLK = 0x6,
    BIRSP_I = 0x0, BIRSP_S = 0x1, BIRSP_E = 0x2,
    BIRSP_I_BLK = 0x4, BIRSP_S_BLK = 0x5, BIRSP_E_BLK = 0x6,
};

int cxl_bisnp_bits(CxlFlitMode mode);     // -1 for 68B
int cxl_birsp_bits(CxlFlitMode mode);
bool cxl_bisnp_encode(CxlFlitMode mode, const CxlBiMsg& m, CxlPacked* out,
                      std::string* err = nullptr);
bool cxl_bisnp_decode(CxlFlitMode mode, const CxlPacked& in, CxlBiMsg* m,
                      std::string* err = nullptr);
bool cxl_birsp_encode(CxlFlitMode mode, const CxlBiMsg& m, CxlPacked* out,
                      std::string* err = nullptr);
bool cxl_birsp_decode(CxlFlitMode mode, const CxlPacked& in, CxlBiMsg* m,
                      std::string* err = nullptr);

// ------------------------------------------------------------- HDM-DB model
struct CxlHdmDbSystem {
    struct Line {
        bool valid = false;
        std::array<std::uint8_t, 64> data{};
        std::uint32_t emd = 0;
    };

    CxlFlitMode mode = CxlFlitMode::B68;
    bool bi_capable = true;
    std::map<std::uint64_t, Line> device_mem;
    std::map<std::uint64_t, Line> host_cache;

    std::uint64_t bisnp_sent = 0, birsp_rcvd = 0, host_invalidations = 0;
    std::uint64_t emd_updates = 0, unsupported_68 = 0, protocol_errors = 0;
    std::uint64_t wire_msgs = 0, wire_errors = 0;

    bool init(CxlFlitMode m = CxlFlitMode::HBR256, std::string* err = nullptr);

    bool device_write(std::uint64_t addr, const void* data, std::uint32_t emd,
                      std::string* err = nullptr);
    bool device_read(std::uint64_t addr, void* data, std::uint32_t* emd,
                     std::string* err = nullptr);
    bool host_write(std::uint64_t addr, const void* data, std::uint32_t emd,
                    std::string* err = nullptr);
    bool host_read(std::uint64_t addr, void* data, std::uint32_t* emd,
                   std::string* err = nullptr);

    // Red self-check hook: respond with a BIRsp without a pending BISnp.
    bool inject_unrequested_birsp = false;
};

// -------------------------------------------------- multi-host HDM-DB (P3.2)
// Each host has its own cache; the device keeps a per-line sharer directory.
// Device writes and host writes invalidate all other sharers with BISnp and
// collect BIRspI before updating device memory.
struct CxlHdmDbMultiHost {
    struct HostLine {
        bool valid = false;
        bool dirty = false;
        std::array<std::uint8_t, 64> data{};
        std::uint32_t emd = 0;
    };
    struct Host {
        std::map<std::uint64_t, HostLine> cache;
        std::uint64_t reads = 0, writes = 0, invalidations = 0;
    };

    CxlFlitMode mode = CxlFlitMode::HBR256;
    bool bi_capable = true;
    std::vector<Host> hosts;
    std::map<std::uint64_t, CxlHdmDbSystem::Line> device_mem;
    std::map<std::uint64_t, std::set<int>> sharers;

    std::uint64_t bisnp_sent = 0, birsp_rcvd = 0, host_invalidations = 0;
    std::uint64_t emd_updates = 0, unsupported_68 = 0, protocol_errors = 0;
    std::uint64_t wire_msgs = 0, wire_errors = 0, host_writebacks = 0;
    std::size_t max_directory_lines = 4096;
    std::uint64_t directory_overflow = 0;

    bool fault_skip_snoop = false;       // red hook
    bool fault_skip_writeback = false;   // kept for symmetry/test coverage

    bool init(int n, CxlFlitMode m = CxlFlitMode::HBR256, std::string* err = nullptr);
    bool device_write(std::uint64_t addr, const void* data, std::uint32_t emd,
                      std::string* err = nullptr);
    bool device_read(std::uint64_t addr, void* data, std::uint32_t* emd,
                     std::string* err = nullptr);
    bool host_write(int host, std::uint64_t addr, const void* data,
                    std::uint32_t emd, std::string* err = nullptr);
    bool host_read(int host, std::uint64_t addr, void* data,
                   std::uint32_t* emd, std::string* err = nullptr);
    // Write-back cache path: dirty data is retained in the host and written
    // back on flush/eviction (P4.6).
    bool host_write_cached(int host, std::uint64_t addr, const void* data,
                           std::uint32_t emd, std::string* err = nullptr);
    bool flush_host_line(int host, std::uint64_t addr, std::string* err = nullptr);
    bool flush_dirty(std::uint64_t addr, std::string* err = nullptr);

    std::size_t sharer_count(std::uint64_t addr) const;

private:
    bool invalidate_hosts(std::uint64_t addr, int except_host, std::string* err);
    bool send_bisnp(int host, std::uint64_t addr, std::uint8_t opcode,
                    std::string* err);
    bool dir_admit(std::uint64_t addr, std::string* err);
};

} // namespace cxl_tlm
