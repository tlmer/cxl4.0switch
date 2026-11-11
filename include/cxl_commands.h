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
//     0401h Get Log (CEL + Component State Dump Log)
//     0600h Perform Maintenance (background)
//     4100h/4101h Get/Set Partition Info
//     4102h/4103h Get/Set LSA
//     4203h/4204h Get/Set Shutdown State
//     4301h Inject Poison
//   2026-11-04 additions:
//     0102h/0103h Get/Set Event Interrupt Policy
//     0402h/0403h Get Log Capabilities / Clear Log
//     0404h Populate Log (synchronous Component State Dump Log)
//     0405h Get Supported Logs Sub-List
//     0500h/0501h/0502h Get Supported Features / Get Feature / Set Feature
//     4400h/4401h Sanitize / Secure Erase
//     4500h/4501h/4502h/4503h/4504h/4505h security commands
//
//   Payload field offsets follow the spec tables; fields beyond the implemented
//   set are returned as zero (documented, not silently invented).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_dcd.h"
#include "cxl_ecs.h"
#include "cxl_event_notification.h"
#include "cxl_event_policy.h"
#include "cxl_features.h"
#include "cxl_log_caps.h"
#include "cxl_mailbox.h"
#include "cxl_media_test.h"
#include "cxl_security.h"
#include "cxl_security_mailbox.h"
#include "cxl_sparing.h"

#include <array>
#include <cstdint>
#include <functional>
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

    // Device timestamp (Get/Set Timestamp 0x0300/0x0301), raw 64-bit value.
    std::uint64_t timestamp = 0;

    // Poison injection limit (P1).
    std::uint32_t poison_limit = 16;
    std::uint64_t poison_clears = 0;      // Clear Poison hits
    std::uint64_t poison_clear_miss = 0;  // Clear Poison on a clean DPA

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

    // Component State Dump Log (CXL 4.0 8.2.10.5.2.3, UUID
    // b3fab4cf-01b6-4332-943e-5e9962f23567).  Manual Populate Log is the only
    // supported population method (no auto-populate trigger).  The log starts
    // empty: Get Log offset 0 returns Retry Required until Populate Log runs.
    // `state_dump_log` carries the 0x40-byte Table 8-256 header plus the
    // vendor dump data.  `state_dump_generation` detects changes between
    // offset-0 fetches for the Interrupted rule.
    static constexpr std::uint8_t kStateDumpUuid[16] = {
        0xb3, 0xfa, 0xb4, 0xcf, 0x01, 0xb6, 0x43, 0x32,
        0x94, 0x3e, 0x5e, 0x99, 0x62, 0xf2, 0x35, 0x67
    };
    std::vector<std::uint8_t> state_dump_log;
    std::uint64_t state_dump_generation = 0;
    bool state_dump_fetch_open = false;
    std::uint64_t state_dump_fetch_generation = 0;
    void populate_state_dump();
    void clear_state_dump();

    // Vendor Debug Log (CXL 4.0 Table 8-250,
    // 5e1819d9-11a9-400c-811f-d60719403d86).  Optional and disabled by
    // default; enabling registers the log with Clear|Populate.  Content is
    // vendor-specific and model-defined, never presented as a standard log.
    static constexpr std::uint8_t kVendorDebugUuid[16] = {
        0x5e, 0x18, 0x19, 0xd9, 0x11, 0xa9, 0x40, 0x0c,
        0x81, 0x1f, 0xd6, 0x07, 0x19, 0x40, 0x3d, 0x86
    };
    bool vendor_debug_log_enabled = false;
    std::vector<std::uint8_t> vendor_debug_log;
    std::uint64_t vendor_debug_clears = 0, vendor_debug_populates = 0;
    void enable_vendor_debug_log();
    void populate_vendor_debug();
    void clear_vendor_debug();

    // Health (CXL 3.2 8.2.10.9.3.1 / Table 8-148):
    //   0x00 health, 0x01 media, 0x02 additional status, 0x03 life used,
    //   0x04 temperature, 0x06 dirty-shutdown count, 0x0A corrected
    //   volatile errors, 0x0E corrected persistent errors.
    std::uint8_t health_status = 0;
    std::uint8_t media_status = 0;
    std::uint8_t additional_status = 0;
    std::uint8_t life_used = 0;
    std::int16_t temperature_c = 30;
    std::uint32_t corrected_volatile_errors = 0;
    std::uint32_t corrected_persistent_errors = 0;

    // Get/Set Alert Configuration (0x4201/0x4202, CXL 4.0 8.2.10.9.3).
    // Get byte0 Valid Alerts = which returned fields are valid;
    // Get byte1 Programmable Alerts = which alerts the host may program.
    // Set input carries Valid Alert Actions and Enable Alert Actions; the
    // latter is internal state and is not returned by Get.
    // The critical thresholds are device capabilities; the warning
    // thresholds are host-programmable.  Temperatures are signed Celsius.
    std::uint8_t alert_valid_fields = 0x1F;
    std::uint8_t alert_programmable = 0x1F;
    std::uint8_t alert_valid = 0;         // last Set Valid Alert Actions
    std::uint8_t alert_enable = 0;        // Enable Alert Actions state
    std::uint8_t life_used_critical = 100;
    std::uint8_t life_used_warn = 80;
    std::int16_t over_temp_critical_c = 85;
    std::int16_t under_temp_critical_c = 0;
    std::int16_t over_temp_warn_c = 80;
    std::int16_t under_temp_warn_c = 5;
    std::uint16_t corrected_volatile_warn = 0;
    std::uint16_t corrected_persistent_warn = 0;
    void reset_alert_config();               // Conventional Reset defaults

    // Event logs: 0 info, 1 warning, 2 failure, 3 fatal, 4 Dynamic Capacity.
    // A per-class capacity bounds the vector; when it overflows the oldest
    // record is dropped and the Linux cxl_get_event_payload overflow state is
    // updated.  event_capacity == 0 means unlimited for classes 0..3; class 4
    // is bounded by dcd_event_log_size (Identify Memory Device 0x43).
    std::vector<CxlEventRecord> events[5];
    std::uint16_t next_handle = 1;
    std::size_t event_capacity = 64;
    std::uint16_t event_overflow_count[5] = {};
    std::uint64_t event_first_overflow[5] = {};
    std::uint64_t event_last_overflow[5] = {};
    // Get Event Records paging cursor per log class: the host repeats the
    // command until More Records clears (CXL 4.0 Table 8-232).  A new event
    // or a clear restarts the cursor; after the final page it resets to 0.
    std::size_t event_cursor[5] = {};

    // Dynamic Capacity Event Log (class 4, CXL 4.0 Table 8-229).  The UUID is
    // ca95afa7-f183-4018-8c2f-95268e101a2a; records are Informational and
    // carry one extent plus the More flag for multi-extent grouping.
    static constexpr std::uint8_t kDcdEventUuid[16] = {
        0xca, 0x95, 0xaf, 0xa7, 0xf1, 0x83, 0x40, 0x18,
        0x8c, 0x2f, 0x95, 0x26, 0x8e, 0x10, 0x1a, 0x2a
    };
    std::uint16_t dcd_event_log_size = 8;   // Identify 0x43; 0 = absent
    // Event types (Table 8-229).
    static constexpr std::uint8_t kDcdEventAdd = 0x00;
    static constexpr std::uint8_t kDcdEventRelease = 0x01;
    static constexpr std::uint8_t kDcdEventForcedRelease = 0x02;
    static constexpr std::uint8_t kDcdEventRegionUpdated = 0x03;
    void add_dcd_event(std::uint8_t event_type,
                       const CxlDcdExtent& extent, bool more,
                       std::uint8_t region_index = 0xFF,
                       std::uint16_t host_id = 0);
    // Platform notification for an updated DCD region (event type 0x03).
    // The model has no mailbox command that reconfigures regions; a platform
    // that changes CxlDcdState::regions calls this to keep the guest's
    // Dynamic Capacity Event Log coherent.
    void notify_dcd_region_updated(std::uint8_t region_index,
                                   std::uint16_t host_id = 0);
    // Platform notification for a forced Dynamic Capacity release (type 0x02).
    void notify_dcd_forced_release(const CxlDcdExtent& extent,
                                   std::uint16_t host_id = 0);


    // Poison list.
    std::vector<CxlPoisonEntry> poison;
    static constexpr std::size_t kPoisonMax = 64;
    // Get Poison List flags bit1 (Poison List Overflow) and the 64-bit
    // overflow timestamp at output offset 0x02 (CXL 3.x; Linux
    // struct cxl_mbox_poison_out).  Latched at the first dropped record,
    // using the device timestamp; Get Poison List never clears either.
    bool poison_overflow = false;
    std::uint64_t poison_overflow_ts = 0;
    // Get Poison List paging/range state (CXL 4.0 Table 8-320).  A new range
    // or Restart Request resets the cursor; Get itself does not clear the
    // list.
    std::size_t poison_cursor = 0;
    std::uint64_t poison_range_start = 0;
    std::uint64_t poison_range_units = 0;
    // Identify Memory Device offset 0x41 bit0 ("Injects Persistent Poison",
    // CXL 4.0 Table 8-309).  When clear, a Conventional/CXL Reset clears
    // injected poison and the poison-list overflow state.
    bool injects_persistent_poison = false;

    // Scan Media (0x4304) / Get Scan Media Results (0x4305).
    // `scan_has_run` follows the CXL rule: Get Scan Media Results returns
    // Unsupported until a Scan Media command has run since the last
    // Conventional Reset.  `scan_running` is the background-operation state;
    // the owner calls complete_scan_media() when the scan finishes.
    struct CxlScanMediaRecord {
        std::uint64_t dpa = 0;
        std::uint32_t length_64b = 0;
    };
    bool scan_has_run = false;
    bool scan_running = false;
    std::uint64_t scan_start_dpa = 0;
    std::uint64_t scan_length_64b = 0;
    std::vector<CxlScanMediaRecord> scan_results;
    // Page cursor for repeated Get Scan Media Results calls: the spec tells
    // the host to repeat until the More flag clears.  Reset on a new scan and
    // after the final page.
    std::size_t scan_result_cursor = 0;
    void complete_scan_media();

    // SLD QoS Telemetry (CXL 4.0 8.2.10.9.8, opcodes 0x4700-0x4702).
    // Identify Memory Device byte 0x42 advertises both optional mechanisms;
    // the control/status values follow Tables 8-343/8-344.  This is a model
    // profile: the percentages are host-programmed thresholds fixed at their
    // spec defaults and the status is a deterministic snapshot.
    std::uint8_t sld_qos_caps = 0x03;         // Egress Congestion + TTR
    std::uint8_t sld_qos_control = 0x00;
    std::uint8_t sld_egress_moderate_pct = 10;
    std::uint8_t sld_egress_severe_pct = 25;
    std::uint8_t sld_backpressure_sample = 8;
    std::uint8_t sld_qos_status = 0;          // average congestion percent

    // Media Operation (0x4402) owner hooks.  The command layer validates the
    // DPA range list; the media/content owner performs the actual operation.
    // Absent callbacks still yield Success (protocol-only profile).
    std::function<void(std::uint64_t dpa, std::uint64_t len)> media_sanitize_range;
    std::function<void(std::uint64_t dpa, std::uint64_t len)> media_zero_range;
    std::uint64_t media_sanitize_ranges = 0, media_zero_ranges = 0;

    // Optional owner media-scan backend for Scan Media (0x4304).  When
    // installed, the returned records are merged with the poison-list
    // fallback.  A record must be 64-byte aligned and lie inside the scanned
    // range; invalid records are dropped and counted in media_scan_invalid.
    // A record that extends past the range end is clamped.  Returning false
    // leaves the poison-list fallback as the only source.
    std::function<bool(std::uint64_t dpa, std::uint64_t len,
                       std::vector<CxlScanMediaRecord>* out)> media_scan_range;
    std::uint64_t media_scan_ranges = 0, media_scan_invalid = 0;

    // Optional Dynamic Capacity Device profile (0x4800-0x4803).  Disabled by
    // default; the platform enables it when the device advertises DCD.
    bool dcd_supported = false;
    CxlDcdState dcd;
    void enable_dcd();

    // Optional Media Test Capability / Results logs (CXL 4.0 8.2.10.5.2.5/6).
    // Disabled by default.  Enabling registers the three Table 8-250 logs;
    // test execution results are supplied by the owner through media_test.
    bool media_test_logs_enabled = false;
    CxlMediaTestState media_test;
    void enable_media_test_logs();
    void refresh_media_test_log_sizes();
    // Parse the Table 8-288/8-289 full-transfer Media Test payload.  Only
    // Action=Full Transfer is modeled; chunked transfer is Unsupported.
    bool start_media_test(const std::vector<std::uint8_t>& in,
                          std::uint16_t* rc);

    // Optional DDR5 ECS Log (CXL 4.0 8.2.10.5.2.4, Table 8-257).
    // Read-only; the owner supplies entries and calls enable_ecs_log().
    CxlEcsState ecs;
    void enable_ecs_log();


    // Security / event policy / logs / features (2026-11-04).
    // The security block owns the mailbox-visible state; media effects
    // (sanitize/secure erase) are the owner's responsibility.
    CxlSecurityState security;
    // Optional Security Send/Receive (0x4600/0x4601) owner hook.  The
    // payload is protocol-defined (TCG/SED); when unset the commands stay
    // Unsupported.  The handler returns true when it consumed the command and
    // fills out/rc; returning false falls through to Unsupported.
    std::function<bool(std::uint16_t opcode,
                       const std::vector<std::uint8_t>& in,
                       std::vector<std::uint8_t>* out, std::uint16_t* rc)>
        security_passthrough;
    std::uint64_t security_passthrough_ops = 0;
    // Optional CXL Security Send/Receive envelope (0x4600/0x4601).  Disabled
    // by default; the protocol semantics (e.g. TCG/SED) are owner handlers.
    CxlSecurityMailbox security_mailbox;
    CxlEventInterruptPolicy event_policy;
    // Opcode 0106h is component-originated on MCTP CCI (not a host mailbox
    // request).  The owner can post/take the standard 2-byte notification
    // payload through these helpers.
    CxlEventNotificationQueue event_notifications;
    bool notify_event(std::uint16_t events, std::string* err = nullptr) {
        return event_notifications.post(events, err);
    }
    bool take_event_notification(std::uint16_t* events,
                                 std::string* err = nullptr) {
        return event_notifications.pop(events, err);
    }
    CxlLogCaps log_caps;                     // CEL registered in ensure_defaults()
    CxlFeatureTable features;
    CxlSparing sparing;                      // CXL 3.2 memory sparing features
    static constexpr std::uint8_t kModelFeatureUuid[16] = {
        0x98, 0x1e, 0x4d, 0x4f, 0x44, 0x45, 0x4c, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    };

    // Statistics.
    std::uint64_t commands = 0, invalid = 0, unsupported = 0;
    std::uint64_t media_disabled_rejects = 0;

    void attach(CxlMailbox* m);              // installs the command handler
    void ensure_defaults();                  // LSA/CEL/model feature defaults
    void apply_reset();                      // Dirty Shutdown + security lock
    // Owner hook: call after the media operation finishes.  Clears the
    // background status, enables media, and deletes the event logs (CXL
    // requires Sanitize to delete logs but not to reset usage counters).
    void complete_sanitize();

    CxlEventRecord make_event(int log_class, std::uint8_t severity,
                              std::uint64_t ts, std::uint16_t handle = 0);
    bool add_event(int log_class, const CxlEventRecord& r);
    void add_poison(std::uint64_t dpa, std::uint32_t length = 1, std::uint32_t type = 0);

    bool handle(std::uint16_t opcode, const std::vector<std::uint8_t>& in,
                std::vector<std::uint8_t>* out, std::uint16_t* rc);
};

} // namespace cxl_tlm
