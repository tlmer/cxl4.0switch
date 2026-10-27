// -----------------------------------------------------------------------------
// cxl_mailbox.h -- CXL Mailbox / CCI register skeleton (CXL 4.0 r1.0 Section
// 8.2.9.4; PCIe MMB for the fields CXL reuses).
//
//   Registers (relative to the mailbox block):
//     +00h Capabilities  (RO)  Payload Size = 1 << bits[4:0]; Type[21:19]
//     +04h Control       (bit0 Doorbell: 1 = command submitted; cleared by
//                              the device on completion)
//     +08h Command       (opcode[15:0], payload length[36:16])
//     +10h Status        (bit0 Background, bit1 MMB Ready, ReturnCode[47:32])
//     +18h Background Command Status
//     +20h Command Payload registers (payload_size bytes)
//
//   Commands are executed synchronously in this skeleton.  The command handler
//   is a callback: payloads/return codes are provided by the owner (M4 loads
//   the real Identify/Health/Poison/Event implementations).  Unsupported
//   opcodes, oversize payloads, and a busy mailbox are rejected with the
//   CXL-defined return codes (Table 8-208) -- never silently.
// -----------------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cxl_tlm {

// CXL-defined return codes (Table 8-208).
enum CxlMbxRc : std::uint16_t {
    MBX_RC_SUCCESS          = 0x0000,
    MBX_RC_BACKGROUND       = 0x0001,
    MBX_RC_INVALID_INPUT    = 0x0002,
    MBX_RC_UNSUPPORTED      = 0x0003,
    MBX_RC_INTERNAL_ERROR   = 0x0004,
    MBX_RC_RETRY            = 0x0005,
    MBX_RC_BUSY             = 0x0006,
    MBX_RC_MEDIA_DISABLED   = 0x0007,
    MBX_RC_FW_IN_PROGRESS   = 0x0008,
    MBX_RC_FW_OUT_OF_ORDER  = 0x0009,
    MBX_RC_INVALID_SLOT     = 0x000B,
    MBX_RC_INVALID_HANDLE   = 0x000E,
    MBX_RC_INVALID_ADDRESS  = 0x000F,
    MBX_RC_POISON_LIMIT     = 0x0010,
    MBX_RC_ABORTED          = 0x0012,
    MBX_RC_INVALID_SECURITY = 0x0013,
    MBX_RC_UNSUPPORTED_MBOX = 0x0015,
    MBX_RC_INVALID_PAYLOAD_LEN = 0x0016,
    MBX_RC_INVALID_LOG      = 0x0017,
};

// Command whitelist (P0 + P1 + PBR switch CCI).
enum CxlMbxCmd : std::uint16_t {
    MBX_CMD_IDENTIFY        = 0x0001,   // component Identify (CCI/switch)
    MBX_CMD_ABORT_BG        = 0x0005,   // Request Abort Background Operation
    MBX_CMD_GET_EVENT_REC   = 0x0100,
    MBX_CMD_CLR_EVENT_REC   = 0x0101,
    MBX_CMD_GET_FW_INFO     = 0x0200,
    MBX_CMD_TRANSFER_FW     = 0x0201,
    MBX_CMD_GET_LOG         = 0x0401,
    MBX_CMD_PERFORM_MAINT   = 0x0600,
    MBX_CMD_IDENTIFY_MEMDEV = 0x4000,   // Identify Memory Device (Type 3)
    MBX_CMD_GET_PARTITION   = 0x4100,
    MBX_CMD_SET_PARTITION   = 0x4101,
    MBX_CMD_GET_LSA         = 0x4102,
    MBX_CMD_SET_LSA         = 0x4103,
    MBX_CMD_GET_HEALTH      = 0x4200,
    MBX_CMD_GET_SHUTDOWN    = 0x4203,
    MBX_CMD_SET_SHUTDOWN    = 0x4204,
    MBX_CMD_GET_POISON      = 0x4300,
    MBX_CMD_INJECT_POISON   = 0x4301,
};

bool cxl_mbx_cmd_supported(std::uint16_t opcode);
const char* cxl_mbx_cmd_name(std::uint16_t opcode);

struct CxlMailbox {
    // Configuration.
    std::uint16_t payload_size = 256;      // 256 B .. 1 MiB (power of two)
    std::uint16_t type = 1;                // 001b = memory device commands
    bool primary = true;                   // secondary: no background ops
    // State.
    bool ready = true;
    bool busy = false;
    bool doorbell = false;
    std::uint16_t opcode = 0;
    std::uint32_t payload_len = 0;
    std::uint16_t return_code = MBX_RC_SUCCESS;
    std::uint16_t background_percent = 0;
    bool background_active = false;
    std::vector<std::uint8_t> payload;

    // Statistics (never silent).
    std::uint64_t commands = 0, unsupported = 0, bad_len = 0, busy_reject = 0;
    std::uint64_t ro_write = 0, ur_count = 0, handler_errors = 0;

    // Owner-provided command implementation.
    using Handler = std::function<bool(std::uint16_t opcode,
                                       const std::vector<std::uint8_t>& req,
                                       std::vector<std::uint8_t>* resp,
                                       std::uint16_t* rc)>;
    Handler handler;

    void init(std::uint16_t payload_bytes = 256, std::uint16_t t = 1, bool is_primary = true);

    std::uint32_t capabilities() const;
    std::uint64_t status() const;
    std::uint64_t background_status() const;

    bool read(std::uint32_t off, unsigned size, std::uint32_t* val,
              std::string* err = nullptr);
    bool write(std::uint32_t off, unsigned size, std::uint32_t val,
               std::string* err = nullptr);

    // Execute the submitted command (called by the doorbell write).
    void execute();

    // Complete a background operation started by a handler that returned
    // MBX_RC_BACKGROUND (status bit0 clears, return code becomes final).
    void complete_background(std::uint16_t rc = MBX_RC_SUCCESS,
                             std::uint16_t percent = 100);
};

} // namespace cxl_tlm
