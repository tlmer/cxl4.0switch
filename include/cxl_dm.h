// -----------------------------------------------------------------------------
// cxl_dm.h -- F1b: minimal CXL Device Management (DM v1) model.
//
//   A model-defined minimal management endpoint: identify, status, health,
//   error log, warm/cold reset and FM-owned mode.  The command set is
//   deliberately small and documented; every unimplemented opcode returns an
//   explicit Unsupported status (never silent).
//
//   Opcodes (model-defined DM v1):
//     0x0001 Identify Device
//     0x0002 Get Device Status
//     0x0003 Get Health Info
//     0x0004 Get Error Log
//     0x0005 Device Reset (input: 1 byte, 0=warm, 1=cold)
//     0x0006 Set Device Mode (input: 1 byte, bit0 FM-owned)
//     0x0007 Get Device Mode
// -----------------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

enum : std::uint16_t {
    DM_RC_OK = 0,
    DM_RC_UNSUPPORTED = 1,
    DM_RC_INVALID = 2,
    DM_RC_BUSY = 3,
};

struct CxlDmDeviceInfo {
    std::uint16_t vid = 0x1E98;
    std::uint16_t did = 0x0003;
    std::uint8_t fw_major = 1, fw_minor = 0;
    std::uint64_t serial = 0x0000C0000001ull;
    std::string model = "CXL-DM-MODEL";
};

struct CxlDmHealth {
    std::uint16_t temperature_c = 40;
    std::uint8_t life_used = 0;          // percent
    std::uint8_t wear_level = 0;         // percent
    std::uint32_t media_errors = 0;
    std::uint32_t corrected_errors = 0;
    std::uint32_t uncorrected_errors = 0;
};

struct CxlDm {
    bool present = true;
    bool ready = true;
    bool fm_owned = false;
    bool reset_required = false;
    CxlDmDeviceInfo info;
    CxlDmHealth health;
    std::uint64_t commands = 0, unsupported = 0, invalid = 0, resets = 0;
    std::uint16_t last_error = 0;

    bool handle(std::uint16_t opcode, const std::vector<std::uint8_t>& in,
                std::vector<std::uint8_t>* out, std::uint16_t* rc);
    bool device_reset(bool cold, std::string* err = nullptr);
    bool set_fm_owned(bool owned, std::string* err = nullptr);
    void inject_error(std::uint32_t corrected, std::uint32_t uncorrected);
    void set_health(std::uint16_t temperature_c, std::uint8_t life_used,
                    std::uint8_t wear_level, std::uint32_t media_errors);
};

} // namespace cxl_tlm
