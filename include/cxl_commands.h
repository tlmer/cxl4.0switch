// -----------------------------------------------------------------------------
// cxl_commands.h -- CXL Type-3 mailbox command implementations (M4.1/M4.2/M4.3).
//
//   Implemented commands (CXL 4.0 r1.0 Section 8.2.10):
//   P0:
//     0001h Identify (component)
//     4000h Identify Memory Device
//     0200h Get FW Info
//     4200h Get Health Info
//     4300h Get Poison List
//     0100h Get Event Records
//     0101h Clear Event Records
//   P1:
//     0005h Request Abort Background Operation
//     0201h Transfer FW
//     0401h Get Log (CEL)
//     0600h Perform Maintenance (background)
//     4100h/4101h Get/Set Partition Info
//     4102h/4103h Get/Set LSA
//     4203h/4204h Get/Set Shutdown State
//     4301h Inject Poison
//
//   Payload field offsets follow the spec tables; fields beyond the implemented
//   set are returned as zero (documented, not silently invented).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_mailbox.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlEventRecord {
    std::uint16_t handle = 0;
    std::uint16_t related = 0;
    std::uint8_t  severity = 0;      // 0=info, 1=warning, 2=failure, 3=fatal
    std::uint64_t timestamp = 0;
    std::array<std::uint8_t, 16> uuid{};
    std::array<std::uint8_t, 0x50> data{};   // event-record-specific payload
    std::vector<std::uint8_t> serialize() const;   // 0x80 bytes
};

struct CxlPoisonEntry {
    std::uint64_t dpa = 0;
    std::uint32_t length = 1;        // in 64 B units
    std::uint32_t type = 0;          // 0=media, 1=external, 2=injected
};

struct CxlCelEntry {
    std::uint16_t opcode = 0;
    std::uint16_t effects = 0;       // Table 8-254 effect bits
};

// Small register-level submit helper shared by the Linux proxy and tests.
// Returns the CXL return code; `out` receives the payload upon success.
std::uint16_t cxl_mbx_submit(CxlMailbox& m, std::uint16_t opcode,
                             const std::vector<std::uint8_t>& in,
                             std::vector<std::uint8_t>* out,
                             std::size_t out_bytes = 0);

struct CxlType3Commands {
    CxlMailbox* mbx = nullptr;

    // Identity / capacity.
    std::uint16_t vid = 0x1E98, did = 0x0001;
    std::uint16_t ssvid = 0x1E98, ssid = 0x0001;
    std::uint64_t serial = 0x1122334455667788ull;
    std::uint8_t  component_type = 0x03;    // Type 3 Device
    std::uint64_t total_capacity_256m = 64; // 16 GB
    std::uint64_t volatile_capacity_256m = 64;
    std::uint64_t persistent_capacity_256m = 0;
    std::uint64_t next_volatile_256m = 0, next_persistent_256m = 0;
    std::uint64_t partition_alignment_256m = 1;   // Set Partition granularity
    std::vector<std::uint8_t> lsa;                 // Label Storage Area
    std::uint32_t lsa_size = 0x10000;
    char fw_revision[16] = "CXL-SIM-FW1.0";
    std::uint8_t active_fw_slot = 1, staged_fw_slot = 0;

    // Label/partition support (P1).
    bool partition_supported = true;
    bool lsa_supported = true;

    // Shutdown state (P1).
    bool shutdown_dirty = false;
    std::uint64_t dirty_shutdown_count = 0;

    // Poison injection limit (P1).
    std::uint32_t poison_limit = 16;

    // Firmware transfer state (P1).
    bool fw_transfer_active = false;
    std::uint8_t fw_transfer_slot = 0;
    std::uint32_t fw_next_offset_128b = 0;
    std::vector<std::uint8_t> fw_image;

    // Command Effects Log (P1).
    std::vector<CxlCelEntry> cel;
    static constexpr std::uint8_t kCelUuid[16] = {
        0x0d, 0xa9, 0xc0, 0xb5, 0xbf, 0x41, 0x4b, 0x78,
        0x8f, 0x79, 0x96, 0xb1, 0x62, 0x3b, 0x3f, 0x17
    };

    // Health.
    std::uint8_t health_status = 0;
    std::uint8_t media_status = 0;
    std::uint8_t life_used = 0;
    std::uint8_t temp_status = 0;
    std::int16_t temperature_c = 30;

    // Event logs: 0 info, 1 warning, 2 failure, 3 fatal.
    std::vector<CxlEventRecord> events[4];
    std::uint16_t next_handle = 1;

    // Poison list.
    std::vector<CxlPoisonEntry> poison;
    static constexpr std::size_t kPoisonMax = 64;

    // Statistics.
    std::uint64_t commands = 0, invalid = 0, unsupported = 0;

    void attach(CxlMailbox* m);              // installs the command handler
    void ensure_defaults();                  // LSA storage + CEL entries
    void apply_reset();                      // Dirty Shutdown Count logic

    CxlEventRecord make_event(int log_class, std::uint8_t severity,
                              std::uint64_t ts, std::uint16_t handle = 0);
    bool add_event(int log_class, const CxlEventRecord& r);
    void add_poison(std::uint64_t dpa, std::uint32_t length = 1, std::uint32_t type = 0);

    bool handle(std::uint16_t opcode, const std::vector<std::uint8_t>& in,
                std::vector<std::uint8_t>* out, std::uint16_t* rc);
};

} // namespace cxl_tlm
