// -----------------------------------------------------------------------------
// cxl_tph.h -- TPH Requester (Steering Tag) data-plane model.
//
//   The TPH extended capability (model/pcie_ext_caps.h) exposes the
//   capability/control/ST table.  This model applies the steering tag to a
//   request:
//     - No ST mode           : no tag is attached.
//     - Interrupt Vector mode: the tag is the MSI-X vector supplied by the
//                              caller (low 16 bits).
//     - Device Specific mode : the tag is an entry of the steering-tag table
//                              (the caller selects the table index).
//   A request is tagged only when Control.En (bits [9:8]) is non-zero and the
//   selected mode is enabled.
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <vector>

namespace cxl_tlm {

struct CxlTph {
    enum Mode : std::uint8_t { NONE = 0, INTERRUPT_VECTOR = 1, DEVICE_SPECIFIC = 2 };

    Mode mode = NONE;
    bool request_enable = false;
    std::vector<std::uint16_t> st_table;

    std::uint64_t requests = 0, tagged = 0, untagged = 0, rejected = 0;

    // Control [2:0] ST mode, [9:8] TPH Requester Enable.
    bool write_control(std::uint32_t ctl) {
        const std::uint32_t m = ctl & 0x7u;
        if (m == 0) mode = NONE;
        else if (m == 1) mode = INTERRUPT_VECTOR;
        else if (m == 2) mode = DEVICE_SPECIFIC;
        else { ++rejected; return false; }       // reserved mode
        request_enable = (ctl & 0x300u) != 0;
        return true;
    }

    bool set_table_entry(std::size_t i, std::uint16_t tag) {
        if (i >= 64) { ++rejected; return false; }
        if (st_table.size() <= i) st_table.resize(i + 1, 0);
        st_table[i] = tag;
        return true;
    }

    // Steering tag for the next request.  `vector` is used in interrupt
    // vector mode; `index` selects the ST table entry in device-specific
    // mode.  Returns false when the request stays untagged.
    bool steer(std::uint32_t vector, std::uint32_t index,
               std::uint16_t* tag) {
        ++requests;
        if (!request_enable || mode == NONE) { ++untagged; return false; }
        std::uint16_t t = 0;
        if (mode == INTERRUPT_VECTOR) {
            t = (std::uint16_t)(vector & 0xFFFFu);
        } else {
            if (index >= st_table.size()) { ++rejected; return false; }
            t = st_table[index];
        }
        ++tagged;
        if (tag) *tag = t;
        return true;
    }
};

} // namespace cxl_tlm
