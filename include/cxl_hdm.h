// -----------------------------------------------------------------------------
// cxl_hdm.h -- CXL HDM Decoder capability structure (CXL 4.0 r1.0 Section
// 8.2.4.20, Tables 8-115..8-126).
//
//   Registers are addressed by their offset relative to the HDM block
//   (model BAR0 + 0x5000; see cxl_reg_map.h).
//
//   Commit model:
//     base/size/control/target-list writes are staged; setting Control.Commit
//     validates and activates the decoder.  Errors set Error Not Committed and
//     leave the decoder inactive (never silent).  Clearing Commit deactivates.
//
//   v1 scope: 1-way decode is complete; multi-way target selection is modeled,
//   but DPA de-interleave is a P1/M3 item (documented).
// -----------------------------------------------------------------------------
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

enum class CxlHdmKind : std::uint8_t { DEVICE = 0, USP = 1, HOST_BRIDGE = 2 };

struct CxlHdm {
    struct Dec {
        // staged programming
        std::uint64_t base = 0, size = 0, dpa_skip = 0;
        std::uint32_t ig = 0;         // Interleave Granularity (0 => 256B)
        std::uint32_t iw = 0;         // Interleave Ways (0 => 1 way)
        bool lock_on_commit = false;
        bool target_host = false;     // Target Range Type: 1 = HDM-H
        bool bi = false, uio = false;
        std::array<std::uint8_t, 8> tgt{};
        bool commit_req = false;
        // active/status
        bool committed = false, err = false, locked = false;
    };

    CxlHdmKind kind = CxlHdmKind::DEVICE;
    int decoder_count = 1;
    int target_count = 1;
    bool poison_on_error_cap = false;
    std::uint32_t global_ctrl = 0;   // bit0 poison-on-error, bit1 device HDM enable
    std::vector<Dec> dec;

    // Statistics (never silent).
    std::uint64_t commit_errors = 0, overlap_errors = 0, lock_block = 0;
    mutable std::uint64_t decode_miss = 0, decode_disabled = 0, decode_hits = 0;

    void init(CxlHdmKind k, int decoders, int targets, bool poison_cap);

    bool read(std::uint32_t off, std::uint32_t* val, std::string* err = nullptr) const;
    bool write(std::uint32_t off, std::uint32_t val, std::string* err = nullptr);

    bool decode(std::uint64_t hpa, int* target, std::uint64_t* dpa,
                std::string* err = nullptr) const;
    bool any_committed() const;

    static std::uint32_t dec_count_field(int n);     // 0h=1 .. Ch=32
    static std::uint32_t target_count_field(int n);  // 1/2/4/8
};

// Nearest supported decoder count (1/2/4/6/8/10/12/14/16/20/24/28/32).
int cxl_dec_count(int requested);
int cxl_target_count(int requested);

// IW encoding (Table 8-123): 0=1,1=2,2=4,3=8,4=16,8=3,9=6,Ah=12.
int cxl_hdm_ways(std::uint32_t iw_code);

// HPA -> device-local DPA for a decoder with the given IW encoding at
// 256B << ig granularity (CXL 4.0 Section 9.13.1).
std::uint64_t cxl_hdm_dpa_from_hpa(std::uint64_t hpa, std::uint64_t base,
                                   std::uint32_t ig, std::uint32_t iw_code);

} // namespace cxl_tlm
