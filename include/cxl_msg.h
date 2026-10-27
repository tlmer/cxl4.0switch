// -----------------------------------------------------------------------------
// cxl_msg.h -- CXL.mem message fields, opcode tables, and channel-level packing.
//
//   Source: CXL 4.0 r1.0 Section 3.3:
//     Table 3-34/3-35  M2S Req        (68B 87b / 256B 100b / PBR 120b)
//     Table 3-40/3-41  M2S RwD        (68B 663b / 256B 680b / PBR 700b, incl. 64B BE + 512b data)
//     Table 3-49/3-50  S2M NDR        (68B 30b / 256B 40b / PBR 48b)
//     Table 3-52/3-53  S2M DRS        (68B 552b / 256B 552b / PBR 560b, incl. 512b data)
//
//   Packing convention (documented, to be re-verified against CTL waveforms):
//     bit 0 = Valid; the remaining fields follow the order of the field tables
//     (LSB first), with the Reserved run included where the table lists RSVD.
//     This reproduces exactly the channel message widths published by the CTL
//     constants (87/663/30/552 for the 68B build).
//
//   Reserved fields are written as 0 and checked on decode (never silent).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_core_cfg.h"
#include "cxl_flit.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

// ----------------------------------------------------------- message classes
enum class CxlMsgClass : uint8_t {
    M2S_REQ = 0,
    M2S_RWD,
    S2M_NDR,
    S2M_DRS,
    COUNT
};

const char* cxl_msg_class_name(CxlMsgClass c);

// --------------------------------------------------------------- opcodes
// M2S Req (Table 3-35).  68B-reserved TEE opcodes are flagged.
enum : uint8_t {
    M2S_REQ_MEM_INV       = 0x0,
    M2S_REQ_MEM_RD        = 0x1,
    M2S_REQ_MEM_RD_DATA   = 0x2,
    M2S_REQ_MEM_RD_FWD    = 0x3,
    M2S_REQ_MEM_WR_FWD    = 0x4,
    M2S_REQ_MEM_RD_TEE    = 0x5,   // 256B/PBR only
    M2S_REQ_MEM_RD_DATA_TEE = 0x6, // 256B/PBR only
    M2S_REQ_MEM_INV_TEE   = 0x7,   // 256B/PBR only
    M2S_REQ_MEM_SPEC_RD   = 0x8,
    M2S_REQ_MEM_INV_NT    = 0x9,
    M2S_REQ_MEM_CLN_EVCT  = 0xA,
    M2S_REQ_MEM_INV_P_TEE = 0xB,   // 256B/PBR only
    M2S_REQ_MEM_SPEC_RD_TEE = 0xC, // 256B/PBR only
    M2S_REQ_TE_UPDATE     = 0xD,   // 256B/PBR only
    M2S_REQ_MEM_CLN_EVCT_TEE = 0xE,// 256B/PBR only
    M2S_REQ_MEM_CLN_EVCT_U = 0xF,
};
// M2S RwD (Table 3-41).
enum : uint8_t {
    M2S_RWD_MEM_WR      = 0x1,
    M2S_RWD_MEM_WR_PTL  = 0x2,
    M2S_RWD_BI_CONFLICT = 0x4,
    M2S_RWD_MEM_RD_FILL = 0x5,   // 256B/PBR only
    M2S_RWD_MEM_WR_TEE  = 0x9,   // 256B/PBR only
    M2S_RWD_MEM_WR_PTL_TEE = 0xA,// 256B/PBR only
    M2S_RWD_MEM_RD_FILL_TEE = 0xD,// 256B/PBR only
};
// S2M NDR (Table 3-50).
enum : uint8_t {
    S2M_NDR_CMP          = 0x0,
    S2M_NDR_CMP_S        = 0x1,
    S2M_NDR_CMP_E        = 0x2,
    S2M_NDR_CMP_M        = 0x3,
    S2M_NDR_BI_CONFLICT_ACK = 0x4,
    S2M_NDR_CMP_TEE      = 0x5,  // 256B only
    S2M_NDR_CMP_TEE_S    = 0x6,  // 256B only
    S2M_NDR_CMP_TEE_E    = 0x7,  // 256B only
};
// S2M DRS (Table 3-53).
enum : uint8_t {
    S2M_DRS_MEM_DATA     = 0x0,
    S2M_DRS_MEM_DATA_NXM = 0x1,
    S2M_DRS_MEM_DATA_TEE = 0x2,  // 256B only
};

bool cxl_opcode_supported_68(CxlMsgClass cls, uint8_t opcode);
const char* cxl_opcode_name(CxlMsgClass cls, uint8_t opcode);

// -------------------------------------------------------------- packed bits
// A channel message is a bit vector; bit 0 = Valid.  `bits` is the exact
// channel width (87/663/...), not padded to a byte boundary.
struct CxlPacked {
    std::vector<std::uint8_t> bytes;
    int bits = 0;

    void resize(int nbits);
    bool bit(int i) const;
    void set_bit(int i, bool v);
    std::uint64_t field(int lsb, int width) const;
    void set_field(int lsb, int width, std::uint64_t v);
    std::string hex() const;
};

// ------------------------------------------------------------- message fields
struct CxlMsgFields {
    bool     valid = true;
    uint8_t  opcode = 0;        // MemOpcode (M2S) or Opcode (S2M)
    uint8_t  snp_type = 0;
    uint8_t  meta_field = 0;
    uint8_t  meta_value = 0;
    uint16_t tag = 0;
    uint64_t addr = 0;          // full HPA; addr[5:0] used as applicable
    uint8_t  ld_id = 0;
    uint8_t  tc = 0;
    bool     poison = false;
    uint16_t ckid = 0;          // 13 bits
    bool     trp = false;
    uint16_t spid = 0;          // 12 bits
    uint16_t dpid = 0;          // 12 bits
    uint8_t  dev_load = 0;
    uint64_t be = 0;            // 64 byte enables (RwD)
    std::vector<std::uint8_t> data;   // 64 bytes (RwD / DRS)
};

int cxl_msg_bits(CxlMsgClass cls, CxlFlitMode mode);   // -1 if unavailable

bool cxl_msg_encode(CxlMsgClass cls, CxlFlitMode mode, const CxlMsgFields& f,
                    CxlPacked* out, std::string* err = nullptr);
bool cxl_msg_decode(CxlMsgClass cls, CxlFlitMode mode, const CxlPacked& in,
                    CxlMsgFields* f, std::string* err = nullptr);

std::string cxl_msg_to_string(CxlMsgClass cls, const CxlMsgFields& f);

} // namespace cxl_tlm
