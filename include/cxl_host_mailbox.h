// -----------------------------------------------------------------------------
// cxl_host_mailbox.h -- host-side CXL Mailbox (CCI) command transport.
//
//   The device-side CxlMailbox model implements the register block; this file
//   is the OS/host driver that submits commands through a memory-mapped BAR.
//   It is transport-agnostic: the caller supplies memory read/write callbacks
//   (CXL.io memory transport, local model memory, or a unit-test mock).
//
//   Registers (relative to the mailbox block):
//     +00 Capabilities  (RO)  payload size = 1 << bits[4:0]
//     +04 Control       (WO)  bit0 Doorbell
//     +08 Command       (RW)  opcode[15:0], payload length[31:16]
//     +0C Command High  (RW)  payload length[36:32]
//     +10 Status        (RO)  bit0 Background, bit1 MMB Ready
//     +14 Status High   (RO)  Return Code[15:0]
//     +20 Command Payload
//
//   The helper never treats a timeout or a transport error as success.  Device
//   return codes are returned verbatim so the caller can apply the CXL rules.
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_mailbox.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlHostMbxIo {
    // Memory-mapped access.  `n` is 1..4 bytes; unaligned accesses are allowed
    // for the payload but register writes are dword writes from the helper.
    std::function<bool(std::uint64_t addr, void* dst, std::size_t n)> read;
    std::function<bool(std::uint64_t addr, const void* src, std::size_t n)> write;
    std::function<void(const std::string&)> log;
    unsigned timeout_polls = 10000;
};

struct CxlHostMbxResult {
    std::uint16_t rc = MBX_RC_INTERNAL_ERROR;
    std::uint16_t payload_size = 0;
    std::uint16_t type = 0;
    std::vector<std::uint8_t> response;
    int polls = 0;
};

// Background operation support: start the command without waiting for the
// Background bit to clear, then poll it.  This is the Linux CCI sequence for
// Perform Maintenance / Transfer FW; `submit` remains for synchronous
// commands.
struct CxlHostMbxBackground {
    bool active = false;
    bool ready = false;
    std::uint16_t rc = 0;
    std::uint8_t percent = 0;
    std::uint32_t raw_status = 0;
    std::uint32_t raw_background = 0;
};
bool cxl_host_mbx_start(const CxlHostMbxIo& io, std::uint64_t base,
                        std::uint16_t opcode,
                        const std::vector<std::uint8_t>& request,
                        std::uint16_t* payload_size = nullptr,
                        std::string* err = nullptr);
bool cxl_host_mbx_poll_background(const CxlHostMbxIo& io, std::uint64_t base,
                                  CxlHostMbxBackground* out,
                                  unsigned max_polls = 10000,
                                  std::string* err = nullptr);
bool cxl_host_mbx_abort_background(const CxlHostMbxIo& io, std::uint64_t base,
                                   CxlHostMbxResult* out = nullptr,
                                   std::string* err = nullptr);

// Submit one mailbox command.  Returns false only for transport/protocol
// failures (bad capabilities, oversize request, timeout, short read/write);
// a device return code is reported in `out.rc` with a true return.
bool cxl_host_mbx_submit(const CxlHostMbxIo& io, std::uint64_t base,
                         std::uint16_t opcode,
                         const std::vector<std::uint8_t>& request,
                         CxlHostMbxResult* out,
                         std::string* err = nullptr);

// Convenience wrappers for the common Linux probe sequence.
bool cxl_host_mbx_identify(const CxlHostMbxIo& io, std::uint64_t base,
                           std::vector<std::uint8_t>* response,
                           CxlHostMbxResult* out = nullptr,
                           std::string* err = nullptr);
bool cxl_host_mbx_health(const CxlHostMbxIo& io, std::uint64_t base,
                         std::vector<std::uint8_t>* response,
                         CxlHostMbxResult* out = nullptr,
                         std::string* err = nullptr);
bool cxl_host_mbx_get_fw_info(const CxlHostMbxIo& io, std::uint64_t base,
                              std::vector<std::uint8_t>* response,
                              CxlHostMbxResult* out = nullptr,
                              std::string* err = nullptr);

// Get/Set Alert Configuration (0x4201/0x4202, CXL r3.2 8.2.10.9.3).
// CXL 4.0 Table 8-316/8-317.  `valid` and `enable_actions` use bits 0..4:
// life-used warning, over-temperature, under-temperature, corrected
// volatile-memory errors, corrected persistent-memory errors.  Get returns
// `valid` (Valid Alerts) and `programmable` (Programmable Alerts); the
// Enable Alert Actions value is Set-only and is not read back by Get.
// Temperatures are signed Celsius.
struct CxlHostMbxAlertConfig {
    std::uint8_t valid = 0;
    std::uint8_t programmable = 0;
    std::uint8_t enable_actions = 0;
    std::uint8_t life_critical = 0;
    std::uint8_t life_warn = 0;
    std::int16_t over_critical_c = 0;
    std::int16_t under_critical_c = 0;
    std::int16_t over_warn_c = 0;
    std::int16_t under_warn_c = 0;
    std::uint16_t corrected_volatile_warn = 0;
    std::uint16_t corrected_persistent_warn = 0;
};
bool cxl_host_mbx_get_alert_config(const CxlHostMbxIo& io, std::uint64_t base,
                                   CxlHostMbxAlertConfig* cfg,
                                   CxlHostMbxResult* out = nullptr,
                                   std::string* err = nullptr);
bool cxl_host_mbx_set_alert_config(const CxlHostMbxIo& io, std::uint64_t base,
                                   const CxlHostMbxAlertConfig& cfg,
                                   CxlHostMbxResult* out = nullptr,
                                   std::string* err = nullptr);

// Scan Media family (0x4303/0x4304/0x4305).  The PA is a byte address
// (64-byte aligned); Length is in 64-byte units (CXL 3.x).  The
// caller owns polling the background operation after scan_media (the model
// device owner calls CxlType3Commands::complete_scan_media()).
struct CxlHostMbxScanResult {
    std::uint64_t dpa = 0;
    std::uint32_t length_64b = 0;
};
bool cxl_host_mbx_get_scan_media_caps(const CxlHostMbxIo& io,
                                      std::uint64_t base,
                                      std::uint64_t dpa,
                                      std::uint64_t length_64b,
                                      std::uint32_t* estimated_ms,
                                      CxlHostMbxResult* out = nullptr,
                                      std::string* err = nullptr);
bool cxl_host_mbx_scan_media(const CxlHostMbxIo& io, std::uint64_t base,
                             std::uint64_t dpa, std::uint64_t length_64b,
                             std::uint8_t flags = 0,
                             CxlHostMbxResult* out = nullptr,
                             std::string* err = nullptr);
bool cxl_host_mbx_get_scan_media_results(
    const CxlHostMbxIo& io, std::uint64_t base,
    std::vector<CxlHostMbxScanResult>* records,
    bool* more = nullptr, CxlHostMbxResult* out = nullptr,
    std::string* err = nullptr);
// Device timestamp (0x0300 Get / 0x0301 Set); value is the raw 64-bit
// device timestamp (nanoseconds in the CXL model profile).
bool cxl_host_mbx_get_timestamp(const CxlHostMbxIo& io, std::uint64_t base,
                                std::uint64_t* timestamp,
                                CxlHostMbxResult* out = nullptr,
                                std::string* err = nullptr);
bool cxl_host_mbx_set_timestamp(const CxlHostMbxIo& io, std::uint64_t base,
                                std::uint64_t timestamp,
                                CxlHostMbxResult* out = nullptr,
                                std::string* err = nullptr);
// Command Effects Log (Get Log with the CEL UUID).
bool cxl_host_mbx_get_cel(const CxlHostMbxIo& io, std::uint64_t base,
                          std::uint32_t offset, std::uint32_t length,
                          std::vector<std::uint8_t>* records,
                          CxlHostMbxResult* out = nullptr,
                          std::string* err = nullptr);

// Get Supported Logs (0x0400): the UUID/size list Linux reads before
// issuing Get Log.  Sizes are the log byte lengths reported by the device.
struct CxlHostMbxLog {
    std::array<std::uint8_t, 16> uuid{};
    std::uint32_t size = 0;
};
bool cxl_host_mbx_get_supported_logs(const CxlHostMbxIo& io,
                                     std::uint64_t base,
                                     std::vector<CxlHostMbxLog>* logs,
                                     CxlHostMbxResult* out = nullptr,
                                     std::string* err = nullptr);
// Get Supported Logs Sub-List (0x0405, CXL 4.0 Table 8-272).  `max_entries`
// must be 1..255; `start` is the zero-based log index.
bool cxl_host_mbx_get_supported_logs_sub_list(
    const CxlHostMbxIo& io, std::uint64_t base,
    std::uint8_t max_entries, std::uint8_t start,
    std::vector<CxlHostMbxLog>* logs, std::uint16_t* total = nullptr,
    CxlHostMbxResult* out = nullptr, std::string* err = nullptr);

// Convenience wrappers for the Linux CCI background flows.  These only encode
// the command; the caller still owns polling/abort.  Return false only for
// transport/protocol failures; device return codes stay in out->rc.
bool cxl_host_mbx_perform_maintenance(const CxlHostMbxIo& io,
                                      std::uint64_t base,
                                      std::uint8_t op,
                                      std::uint8_t sub_op,
                                      CxlHostMbxResult* out = nullptr,
                                      std::string* err = nullptr);
bool cxl_host_mbx_transfer_fw(const CxlHostMbxIo& io, std::uint64_t base,
                              std::uint8_t action, std::uint8_t slot,
                              std::uint32_t offset_128b,
                              const std::vector<std::uint8_t>& data,
                              CxlHostMbxResult* out = nullptr,
                              std::string* err = nullptr);
// Activate FW (0x0202): action 0=online, 1=offline; slot 1..4.  The input is
// the 2-byte struct cxl_mbox_activate_fw (Linux 6.18 cxlmem.h:320).
bool cxl_host_mbx_activate_fw(const CxlHostMbxIo& io, std::uint64_t base,
                              std::uint8_t action, std::uint8_t slot,
                              CxlHostMbxResult* out = nullptr,
                              std::string* err = nullptr);
bool cxl_host_mbx_get_log(const CxlHostMbxIo& io, std::uint64_t base,
                          const std::array<std::uint8_t, 16>& uuid,
                          std::uint32_t offset, std::uint32_t length,
                          std::vector<std::uint8_t>* bytes,
                          CxlHostMbxResult* out = nullptr,
                          std::string* err = nullptr);

// Partition Info (Get/Set).  Capacities are in 256 MB units.
struct CxlHostMbxPartition {
    std::uint64_t volatile_256m = 0;
    std::uint64_t persistent_256m = 0;
    std::uint64_t next_volatile_256m = 0;
    std::uint64_t next_persistent_256m = 0;
};
bool cxl_host_mbx_get_partition(const CxlHostMbxIo& io, std::uint64_t base,
                                CxlHostMbxPartition* part,
                                CxlHostMbxResult* out = nullptr,
                                std::string* err = nullptr);
bool cxl_host_mbx_set_partition(const CxlHostMbxIo& io, std::uint64_t base,
                                std::uint64_t volatile_256m, bool immediate,
                                CxlHostMbxResult* out = nullptr,
                                std::string* err = nullptr);

// Label Storage Area.  Set sends `data` verbatim after the 8-byte header.
bool cxl_host_mbx_get_lsa(const CxlHostMbxIo& io, std::uint64_t base,
                          std::uint32_t offset, std::uint32_t length,
                          std::vector<std::uint8_t>* data,
                          CxlHostMbxResult* out = nullptr,
                          std::string* err = nullptr);
bool cxl_host_mbx_set_lsa(const CxlHostMbxIo& io, std::uint64_t base,
                          std::uint32_t offset,
                          const std::vector<std::uint8_t>& data,
                          CxlHostMbxResult* out = nullptr,
                          std::string* err = nullptr);

// Poison list.  `more` is the device's More Media Error Records flag.
// `source` is the CXL source encoded in address[2:0] (0 unknown, 1 external,
// 2 internal, 3 injected, 7 vendor).  `type` is the model's compatibility
// view (0=media/internal, 1=external, 2=injected, 3=vendor).
struct CxlHostMbxPoison {
    std::uint64_t dpa = 0;
    std::uint32_t length = 0;
    std::uint32_t type = 0;
    std::uint32_t source = 0;
};
// `overflow` (flags bit1) means the device dropped at least one poison
// record; `overflow_ts` is the device timestamp of the first drop.
// `dpa`/`length_64b` are the CXL 4.0 Table 8-320 range (length in 64-byte
// units); a zero length asks the model for the full device range.  `restart`
// sets the Restart Request flag (the model acknowledges it in output bit3).
bool cxl_host_mbx_get_poison(const CxlHostMbxIo& io, std::uint64_t base,
                             std::vector<CxlHostMbxPoison>* records,
                             bool* more = nullptr,
                             CxlHostMbxResult* out = nullptr,
                             std::string* err = nullptr,
                             bool* overflow = nullptr,
                             std::uint64_t* overflow_ts = nullptr,
                             std::uint64_t dpa = 0,
                             std::uint64_t length_64b = 0,
                             bool restart = false);
bool cxl_host_mbx_inject_poison(const CxlHostMbxIo& io, std::uint64_t base,
                                std::uint64_t dpa,
                                CxlHostMbxResult* out = nullptr,
                                std::string* err = nullptr);
// Clear Poison (0x4302): DPA + 64-byte write data; the model consumes the
// write data and removes the DPA from the poison list.
bool cxl_host_mbx_clear_poison(const CxlHostMbxIo& io, std::uint64_t base,
                               std::uint64_t dpa,
                               CxlHostMbxResult* out = nullptr,
                               std::string* err = nullptr);

// Event records (raw 0x80-byte records returned after the 0x20-byte header).
bool cxl_host_mbx_get_events(const CxlHostMbxIo& io, std::uint64_t base,
                             int log_class,
                             std::vector<std::uint8_t>* records, bool* more,
                             CxlHostMbxResult* out = nullptr,
                             std::string* err = nullptr);
bool cxl_host_mbx_clear_events(const CxlHostMbxIo& io, std::uint64_t base,
                               int log_class, bool clear_all,
                               const std::vector<std::uint16_t>& handles,
                               CxlHostMbxResult* out = nullptr,
                               std::string* err = nullptr);

// Shutdown state (dirty bit).  The host sets dirty before a clean shutdown
// and reads it on the next boot.
bool cxl_host_mbx_get_shutdown_state(const CxlHostMbxIo& io, std::uint64_t base,
                                     bool* dirty,
                                     CxlHostMbxResult* out = nullptr,
                                     std::string* err = nullptr);
bool cxl_host_mbx_set_shutdown_state(const CxlHostMbxIo& io, std::uint64_t base,
                                     bool dirty,
                                     CxlHostMbxResult* out = nullptr,
                                     std::string* err = nullptr);

// Locate the mailbox from the Register Locator DVSEC (Linux does this before
// touching the CCI).  The component register block is BIR + offset; the
// mailbox block sits at +0xE000 inside it.
struct CxlHostMbxLoc {
    bool found = false;
    std::uint8_t bir = 0;
    std::uint8_t block_id = 0;
    std::uint64_t bar_offset = 0;
    std::uint64_t addr = 0;       // BAR base + block offset + 0xE000
};
bool cxl_host_mbx_locate(
    const std::function<bool(std::uint16_t off, std::uint32_t* v)>& cfg_rd32,
    std::uint16_t regloc_off, int regloc_blocks,
    const std::uint64_t bar_base[6], CxlHostMbxLoc* out,
    std::string* err = nullptr);

} // namespace cxl_tlm
