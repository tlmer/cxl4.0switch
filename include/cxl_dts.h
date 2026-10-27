// -----------------------------------------------------------------------------
// cxl_dts.h -- runtime DTS template loader (checklist T1).
//
// Supported properties (all optional; defaults preserve the model behavior):
//   compatible = "onetest,cxl-switch";
//   onetest,num-ports = <1..32>;
//   onetest,hdm-base = <address>;
//   onetest,bar0-base = <address>;
//   onetest,mailbox-payload = <256|512|1024>;
//   onetest,flit-mode = "68b" | "256b" | "pbr";
//
// The loader is a small text parser, not a full DTC.  Unknown properties are
// ignored; malformed values and incompatible strings are explicit errors.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_core_cfg.h"

#include <cstdint>
#include <string>

namespace cxl_tlm {

struct CxlDtsConfig {
    std::string compatible = "onetest,cxl-switch";
    int num_ports = 4;
    std::uint64_t hdm_base = 0x80000000ull;
    std::uint64_t bar0_base = 0x80000000ull;
    int mailbox_payload = 1024;
    CxlFlitMode mode = CxlFlitMode::B68;
};

bool cxl_model_load_dts(const std::string& path, CxlDtsConfig* cfg,
                        std::string* err = nullptr);

} // namespace cxl_tlm
