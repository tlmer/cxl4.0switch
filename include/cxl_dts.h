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
//   onetest,root-bus = <0..254>;
//   onetest,root-port-dev = <0..31>;
//   onetest,usp-bus = <1..254>;        root-port secondary bus
//   onetest,usp-sec = <2..254>;        switch internal/DSP bus
//   onetest,auto-enumerate = <0|1>;     default 1 when a DTS is loaded
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

    // Virtual root description (q35 defaults).  A DTS can describe a
    // different virtual root without recompiling the model.
    std::uint8_t root_bus = 0;
    std::uint8_t root_port_dev = 1;
    std::uint8_t usp_bus = 1;      // root-port secondary bus
    std::uint8_t usp_sec = 2;      // switch internal/DSP bus
    bool auto_enumerate = true;    // DTS present => enumerate on init
};

bool cxl_model_load_dts(const std::string& path, CxlDtsConfig* cfg,
                        std::string* err = nullptr);

} // namespace cxl_tlm
