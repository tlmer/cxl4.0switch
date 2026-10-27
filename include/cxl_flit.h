// -----------------------------------------------------------------------------
// cxl_flit.h -- CXL.cachemem flit containers and slot-format contract.
//
//   Sources:
//     CXL 4.0 r1.0 Section 4.2 (68B flit), Figures 4-5..4-7, Tables 4-1..4-8;
//     Section 4.3 (256B flit) for the container layout (M1.10 fills the
//     per-format field maps).
//
//   68B flit (Section 4.2.2):
//     66B link-layer flit = 4 x 16B slots + 2B CRC; plus 2B ARB/MUX = 68B total.
//     The 32-bit Flit Header occupies bytes 0..3 of Slot 0 (the header slot).
//     All multi-byte fields are little-endian; bit 0 = LSB of each byte.
//
//   Reserved rule (Table 4-1 note): undefined bits are cleared by the sender
//   and ignored by the receiver; static control-flit bits are checked.
// -----------------------------------------------------------------------------
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace cxl_tlm {

// ---------------------------------------------------------------- directions
enum class CxlDir : uint8_t { H2D_M2S = 0, D2H_S2M = 1 };

// ------------------------------------------------------------ slot formats
// Table 4-6: the same 3-bit encoding selects Hn for slot 0 and Gn for slots 1..3.
enum class CxlSlotFormat : uint8_t {
    H0 = 0, H1 = 1, H2 = 2, H3 = 3, H4 = 4, H5 = 5, H6 = 6, RSVD = 7
};

enum class CxlSlotKind : uint8_t { HEADER = 0, GENERIC = 1, RESERVED = 2 };

CxlSlotKind slot_kind(CxlDir d, int slot_index, CxlSlotFormat f);
int slot_format_bits(CxlDir d, int slot_index, CxlSlotFormat f);   // -1 = reserved

// --------------------------------------------------------------- 68B flit
constexpr std::size_t kFlit68LinkBytes = 66;   // 64B slots + 2B CRC
constexpr std::size_t kFlit68Bytes     = 68;   // + 2B ARB/MUX

struct CxlFlit68 {
    // slot[0][0..3] = flit header; slot[0..3][0..15] = 4 x 16B slots.
    std::uint8_t slot[4][16];
    std::uint16_t crc = 0;          // link-layer CRC (opaque here)
    std::uint8_t arb_mux[2] = {0, 0};

    void clear() { *this = CxlFlit68(); }

    // -- Flit header accessors (Table 4-1 / Figure 4-7) -------------------
    bool type() const { return (slot[0][0] >> 0) & 1u; }        // 0 = protocol
    bool ak() const   { return (slot[0][0] >> 2) & 1u; }
    bool be() const   { return (slot[0][0] >> 3) & 1u; }
    bool sz() const   { return (slot[0][0] >> 4) & 1u; }
    std::uint8_t req_crd() const { return slot[0][3] & 0x0Fu; }
    std::uint8_t data_crd() const { return (slot[0][3] >> 4) & 0x0Fu; }
    std::uint8_t rsp_crd() const { return (slot[0][2] >> 4) & 0x0Fu; }
    CxlSlotFormat slot0_fmt() const {
        return static_cast<CxlSlotFormat>((slot[0][0] >> 5) & 0x7u);
    }
    CxlSlotFormat slot1_fmt() const {
        return static_cast<CxlSlotFormat>(slot[0][1] & 0x7u);
    }
    CxlSlotFormat slot2_fmt() const {
        return static_cast<CxlSlotFormat>((slot[0][1] >> 3) & 0x7u);
    }
    CxlSlotFormat slot3_fmt() const {
        const std::uint8_t lo = (slot[0][1] >> 6) & 0x3u;
        const std::uint8_t hi = (slot[0][2] >> 3) & 0x1u;
        return static_cast<CxlSlotFormat>((hi << 2) | lo);
    }
    CxlSlotFormat slot_fmt(int i) const {
        switch (i) { case 0: return slot0_fmt(); case 1: return slot1_fmt();
                     case 2: return slot2_fmt(); default: return slot3_fmt(); }
    }

    void set_type(bool v) { set_bit(slot[0][0], 0, v); }
    void set_ak(bool v)   { set_bit(slot[0][0], 2, v); }
    void set_be(bool v)   { set_bit(slot[0][0], 3, v); }
    void set_sz(bool v)   { set_bit(slot[0][0], 4, v); }
    void set_req_crd(std::uint8_t v)  { slot[0][3] = (std::uint8_t)((slot[0][3] & 0xF0u) | (v & 0x0Fu)); }
    void set_data_crd(std::uint8_t v) { slot[0][3] = (std::uint8_t)((slot[0][3] & 0x0Fu) | ((v & 0x0Fu) << 4)); }
    void set_rsp_crd(std::uint8_t v)  { slot[0][2] = (std::uint8_t)((slot[0][2] & 0x0Fu) | ((v & 0x0Fu) << 4)); }
    void set_slot0_fmt(CxlSlotFormat f) {
        slot[0][0] = (std::uint8_t)((slot[0][0] & 0x1Fu) | ((static_cast<unsigned>(f) & 0x7u) << 5));
    }
    void set_slot1_fmt(CxlSlotFormat f) {
        slot[0][1] = (std::uint8_t)((slot[0][1] & 0xF8u) | (static_cast<unsigned>(f) & 0x7u));
    }
    void set_slot2_fmt(CxlSlotFormat f) {
        slot[0][1] = (std::uint8_t)((slot[0][1] & 0xC7u) | ((static_cast<unsigned>(f) & 0x7u) << 3));
    }
    void set_slot3_fmt(CxlSlotFormat f) {
        const unsigned v = static_cast<unsigned>(f) & 0x7u;
        slot[0][1] = (std::uint8_t)((slot[0][1] & 0x3Fu) | ((v & 0x3u) << 6));
        slot[0][2] = (std::uint8_t)((slot[0][2] & 0xF7u) | (((v >> 2) & 0x1u) << 3));
    }

    // -- Validation -------------------------------------------------------
    // Reserved bits: byte0 bit1 (RV) + byte2 bits[2:0] (RSVD) must be 0.
    // Protocol flits must have type()==0 (control flits are outside this model).
    bool validate(std::string* err = nullptr) const;

private:
    static void set_bit(std::uint8_t& b, int n, bool v) {
        b = (std::uint8_t)(v ? (b | (1u << n)) : (b & ~(1u << n)));
    }
};

// -------------------------------------------------------------- 256B flit
// Section 4.3: 2B HDR + 14B H-slot + 14 x 16B G-slots + 2B CRD + 8B CRC + 6B FEC.
// PBR is a 256B-flit subset whose message headers carry SPID/DPID (PTH); the
// container layout is identical (M5.2 fills the PBR field maps).
constexpr std::size_t kFlit256HdrBytes   = 2;
constexpr std::size_t kFlit256HSlotBytes = 14;
constexpr std::size_t kFlit256GSlots     = 14;
constexpr std::size_t kFlit256GSlotBytes = 16;
constexpr std::size_t kFlit256CrdBytes   = 2;
constexpr std::size_t kFlit256CrcBytes   = 8;
constexpr std::size_t kFlit256FecBytes   = 6;
constexpr std::size_t kFlit256Bytes =
    kFlit256HdrBytes + kFlit256HSlotBytes + kFlit256GSlots * kFlit256GSlotBytes +
    kFlit256CrdBytes + kFlit256CrcBytes + kFlit256FecBytes;

static_assert(kFlit256Bytes == 256, "256B flit container layout must sum to 256 bytes");
static_assert(kFlit68LinkBytes == 4 * 16 + 2, "68B link flit = 4 x 16B slots + 2B CRC");
static_assert(kFlit68Bytes == kFlit68LinkBytes + 2, "68B flit = link flit + 2B ARB/MUX");

const char* flit_mode_name(bool pbr, bool hbr256);   // "PBR" / "256B" / "68B"

} // namespace cxl_tlm
