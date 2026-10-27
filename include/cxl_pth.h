// -----------------------------------------------------------------------------
// cxl_pth.h -- PBR TLP Header (PTH) 32-bit encoding.
//
//   Source: CXL 4.0 r1.0 Section 3.1.8, Table 3-4 (PTH Format):
//     byte0: [7:6] = 11b (PTH), [5:3] = Rsvd (0), [2] = Hie, [1] = DSAR, [0] = PIF
//     byte1: SPID[7:0]
//     byte2: [3:0] = SPID[11:8], [7:4] = DPID[3:0]
//     byte3: DPID[11:4]
//
//   Grammar (same section):
//     bits[7:6] of the first byte: 00b = NOP TLP, 01b = Reserved,
//                                  10b = Local Prefix, 11b = PTH.
//     NOP TLPs and Local Prefixes never carry a PTH; Local Prefixes precede it.
// -----------------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <string>

namespace cxl_tlm {

struct CxlPth {
    std::uint16_t spid = 0;    // 12 bits
    std::uint16_t dpid = 0;    // 12 bits
    bool hie = false;          // Hierarchy ID Enable
    bool dsar = false;         // Downstream Acceptance Rules
    bool pif = false;          // PID Forward
};

// Prefix grammar helpers on the first byte of a TLP/prefix DWORD.
inline bool cxl_is_pth_byte(std::uint8_t b)          { return (b >> 6) == 0x3; }
inline bool cxl_is_nop_tlp_byte(std::uint8_t b)      { return (b >> 6) == 0x0; }
inline bool cxl_is_local_prefix_byte(std::uint8_t b) { return (b >> 6) == 0x2; }
inline bool cxl_is_rsvd_prefix_byte(std::uint8_t b)  { return (b >> 6) == 0x1; }

// Encode/decode the 4-byte PTH (little-endian DWORD layout).
bool cxl_pth_encode(const CxlPth& p, std::uint8_t out[4], std::string* err = nullptr);
bool cxl_pth_decode(const std::uint8_t in[4], CxlPth* p, std::string* err = nullptr);

std::uint32_t cxl_pth_pack(const CxlPth& p);
bool cxl_pth_unpack(std::uint32_t dword, CxlPth* p, std::string* err = nullptr);

std::string cxl_pth_to_string(const CxlPth& p);

} // namespace cxl_tlm
