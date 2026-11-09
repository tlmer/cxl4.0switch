// -----------------------------------------------------------------------------
// cxl_sparing.h -- CXL Memory Sparing features (CXL 3.2 8.2.10.7).
//
//   Layouts and feature UUIDs follow Linux 6.18:
//     include/cxl/features.h          (CXL_FEAT_{CACHELINE,ROW,BANK,RANK}_SPARING_UUID)
//     drivers/cxl/core/edac.c         (Table 8-120 input payload,
//                                      Table 8-134 readable attributes)
//
//   Get Feature payload (19 bytes):
//     u8  max_op_latency
//     u16 op_cap
//     u16 op_mode
//     u8  op_class
//     u8  op_subclass
//     u8  reserved[9]
//     u8  reserved
//     u16 restriction_flags     bit0: safe-when-in-use is NOT supported
//                                bit1: hard sparing supported
//                                bit2: soft sparing supported
//
//   Set Feature payload (14 bytes):
//     u8 flags        bit0 query resource, bit1 hard sparing,
//                     bit2 sub-channel valid, bit3 nibble mask valid
//     u8 channel / rank / bank_group / bank / sub_channel
//     u8 nibble_mask[3]
//     u8 row[3]
//     u16 column
//
//   The model keeps a record per applied repair so a test (or a future
//   mailbox consumer) can observe what the device accepted.  It does not
//   model the actual DRAM spare substitution.
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

enum class CxlSparingKind : std::uint8_t {
    Cacheline = 0,
    Row,
    Bank,
    Rank,
};

struct CxlSparingRecord {
    CxlSparingKind kind = CxlSparingKind::Cacheline;
    std::uint8_t flags = 0;
    std::uint8_t channel = 0;
    std::uint8_t rank = 0;
    std::uint8_t bank_group = 0;
    std::uint8_t bank = 0;
    std::uint8_t sub_channel = 0;
    std::array<std::uint8_t, 3> nibble_mask{};
    std::uint32_t row = 0;
    std::uint16_t column = 0;
};

struct CxlSparing {
    static constexpr std::size_t kGetBytes = 19;
    static constexpr std::size_t kSetBytes = 14;

    bool cap_safe_when_in_use = true;
    bool cap_hard = true;
    bool cap_soft = true;
    std::uint8_t max_op_latency = 0;
    std::uint16_t op_cap = 0x0007;
    std::uint16_t op_mode = 0x0000;
    std::uint8_t op_class = 0;
    std::uint8_t op_subclass = 0;

    std::vector<CxlSparingRecord> records;
    std::uint64_t applied = 0, queries = 0, rejected = 0, bad_payload = 0;

    static const std::array<std::uint8_t, 16>& uuid(CxlSparingKind k);
    static const char* name(CxlSparingKind k);

    // Readable attributes for Get Feature (selection is the model's current
    // attributes for every selection).
    std::vector<std::uint8_t> get_attrs() const;

    // Validate and apply a Set Feature payload.  Returns false when the
    // payload is malformed or a kind-specific field is out of range.
    bool apply(CxlSparingKind k, const std::vector<std::uint8_t>& in);
};

} // namespace cxl_tlm
