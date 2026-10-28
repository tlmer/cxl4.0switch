// -----------------------------------------------------------------------------
// cxl_swdn_io.h -- customer swdn CXL.io payload packing (M6.2b).
//
//   The customer swdn top uses a 771-bit container for each CXL.io source and
//   for radm_payload.  Only the low 449 bits carry fields; bits 770:449 are
//   zero padding.  dbi_dst_payload is a separate 37-bit vector.
//
//   Bit positions are read from the customer RTL:
//     top1/swdn_cxl.v         radm_payload assembly (1516-1549)
//     top1/mem2client0.v      dst0_payload -> CTL client0 (84-93)
//     top1/cfg2dbi.v          radm_dst_payload -> DBI (97-101), DBI FIFO (131-147)
//     top1/dbi2cpl.v          DBI return data
//
//   This header is the model-side single source of truth for that layout.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_msg.h"

#include <array>
#include <cstdint>
#include <string>

namespace cxl_tlm {

// 771-bit container; low 449 bits meaningful, high 322 bits must be zero.
constexpr int kCxlSwdnIoBits = 771;
constexpr int kCxlSwdnIoUsedBits = 449;
constexpr int kCxlSwdnDbiBits = 37;

// Field offsets (LSB position) in the 771-bit container.
enum CxlSwdnIoBit : int {
    CXL_SWDN_IO_ERR          = 0,
    CXL_SWDN_IO_REQID        = 1,    // 16
    CXL_SWDN_IO_POISONED     = 17,
    CXL_SWDN_IO_BYTE_CNT     = 18,   // 12
    CXL_SWDN_IO_DW_LEN       = 30,   // 10; 32:30 + 3 reserved bits at 42:40
    CXL_SWDN_IO_TAG          = 43,   // 8
    CXL_SWDN_IO_FIRST_BE     = 51,   // 4
    CXL_SWDN_IO_LAST_BE      = 55,   // 4
    CXL_SWDN_IO_BCM          = 59,
    CXL_SWDN_IO_RSVD         = 60,   // 15
    CXL_SWDN_IO_CPL_STATUS   = 75,   // 3
    CXL_SWDN_IO_ADDR         = 78,   // 64
    CXL_SWDN_IO_TD           = 142,
    CXL_SWDN_IO_FUNC_NUM     = 143,  // 3
    CXL_SWDN_IO_ATTR         = 146,  // 2
    CXL_SWDN_IO_TC           = 148,  // 3
    CXL_SWDN_IO_TYPE         = 151,  // 5
    CXL_SWDN_IO_FMT          = 156,  // 2
    CXL_SWDN_IO_DATA_LO      = 158,  // 64
    CXL_SWDN_IO_EOT          = 222,
    CXL_SWDN_IO_HV           = 223,
    CXL_SWDN_IO_DV           = 224,
    CXL_SWDN_IO_P2P_MATCH    = 225,
    CXL_SWDN_IO_RSVD2        = 226,  // 7
    CXL_SWDN_IO_APP_BUS_NUM  = 233,  // 8
    CXL_SWDN_IO_REMOTE_REQID = 241,  // 16
    CXL_SWDN_IO_DATA_HI      = 257   // 192
};

// Decoded CXL.io TLP fields.  The data array is 32 bytes, little-endian by
// byte index; data[0..7] maps to bits 221:158 and data[8..31] to 448:257.
struct CxlSwdnIOTlp {
    std::uint8_t  fmt = 0;
    std::uint8_t  type = 0;
    std::uint8_t  tc = 0;
    std::uint8_t  attr = 0;
    std::uint8_t  cpl_status = 0;
    std::uint8_t  func_num = 0;
    std::uint8_t  first_be = 0;
    std::uint8_t  last_be = 0;
    std::uint8_t  tag = 0;
    std::uint8_t  app_bus_num = 0;
    std::uint8_t  rsvd2 = 0;
    bool          td = false;
    bool          poisoned = false;
    bool          bcm = false;
    bool          dv = false;
    bool          hv = false;
    bool          eot = false;
    bool          err = false;
    bool          p2p_match = false;
    std::uint16_t reqid = 0;
    std::uint16_t remote_reqid = 0;
    std::uint16_t rsvd = 0;
    std::uint16_t dw_len = 0;
    std::uint16_t byte_cnt = 0;
    std::uint64_t addr = 0;
    std::array<std::uint8_t, 32> data{};
};

// DBI return FIFO: {ack, wr[3:0], data[31:0]}.
struct CxlSwdnDbi {
    std::uint32_t data = 0;
    std::uint8_t  wr = 0;
    bool          ack = false;
};

bool cxl_swdn_io_pack_tlp(const CxlSwdnIOTlp& t, CxlPacked* out,
                          std::string* err = nullptr);
bool cxl_swdn_io_unpack_tlp(const CxlPacked& in, CxlSwdnIOTlp* t,
                            std::string* err = nullptr);
bool cxl_swdn_io_pack_dbi(const CxlSwdnDbi& d, CxlPacked* out,
                          std::string* err = nullptr);
bool cxl_swdn_io_unpack_dbi(const CxlPacked& in, CxlSwdnDbi* d,
                            std::string* err = nullptr);

} // namespace cxl_tlm
