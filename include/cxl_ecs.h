// -----------------------------------------------------------------------------
// cxl_ecs.h -- DDR5 Error Check Scrub (ECS) Log (CXL 4.0 8.2.10.5.2.4,
// Table 8-257).
//
//   UUID f1720d60-a7a9-4306-a003-11948f9e077c.
//   Layout: 4-byte common header followed by 0x22-byte entries:
//     04h + (n-1)*22h  16 B component identifier
//     14h + (n-1)*22h   2 B ECS configuration (threshold bits[2:0],
//                           codeword/row mode bit[3])
//     16h + (n-1)*22h  16 B error count / address information
//                         (bit0 error found, bits[23:8] error count,
//                          bits[31:24] max row error count,
//                          bits[95:32] address with max errors)
//
//   The model is a log serializer/owner interface; actual DDR5 scrubbing is
//   outside the model.  The log is read-only (no Clear Log support).
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cxl_tlm {

constexpr std::uint8_t kDdr5EcsUuid[16] = {
    0xf1, 0x72, 0x0d, 0x60, 0xa7, 0xa9, 0x43, 0x06,
    0xa0, 0x03, 0x11, 0x94, 0x8f, 0x9e, 0x07, 0x7c,
};

struct CxlEcsEntry {
    std::array<std::uint8_t, 16> component_id{};
    std::uint16_t configuration = 0x0003;  // 256 threshold, row mode
    std::array<std::uint8_t, 16> error_info{};
};

struct CxlEcsState {
    bool enabled = false;
    std::uint8_t entry_type = 0;           // 0 per DRAM, 1 per media FRU
    bool component_id_format = false;      // bit10 of the common header
    std::vector<CxlEcsEntry> entries;

    std::vector<std::uint8_t> build_log() const
    {
        std::vector<std::uint8_t> v(4 + entries.size() * 0x22, 0);
        put32(v, 0, (std::uint32_t)(entries.size() & 0x3FFu) |
                    ((component_id_format ? 1u : 0u) << 10) |
                    ((std::uint32_t)(entry_type & 0x3u) << 11));
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            const std::size_t o = 4 + i * 0x22;
            std::memcpy(v.data() + o, e.component_id.data(), 16);
            put16(v, o + 0x10, e.configuration);
            std::memcpy(v.data() + o + 0x12, e.error_info.data(), 16);
        }
        return v;
    }

private:
    static void put16(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint16_t x)
    {
        v[o] = (std::uint8_t)(x & 0xFF);
        v[o + 1] = (std::uint8_t)(x >> 8);
    }
    static void put32(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint32_t x)
    {
        for (int i = 0; i < 4; ++i) v[o + (std::size_t)i] =
            (std::uint8_t)(x >> (8 * i));
    }
};

} // namespace cxl_tlm
