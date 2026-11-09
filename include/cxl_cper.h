// -----------------------------------------------------------------------------
// cxl_cper.h -- UEFI CPER record for CXL protocol errors (model-side RAS).
//
//   The model previously only accounted for AER/PME messages.  This module
//   builds and parses the CPER record that a CXL Root Port / switch / device
//   would hand to the OS error manager for a CXL Protocol Error Section.
//
//   Layout sources (read-only reference, not compiled into the model):
//     /mnt/data/linux-6.18/include/linux/cper.h  (record/section headers,
//                                                  CXL section GUIDs)
//     /mnt/data/linux-6.18/include/cxl/event.h   (cxl_cper_sec_prot_err)
//     /mnt/data/linux-6.18/drivers/firmware/efi/cper_cxl.c
//
//   Byte layout is little-endian and byte-packed, matching the UEFI tables.
//   Parsing never invents missing data: a short/corrupt record is rejected
//   with an explicit error string.
// -----------------------------------------------------------------------------
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cxl_tlm {

// CPER error_severity / section_severity (UEFI 2.7 N.2.1).
enum CxlCperSeverity : std::uint32_t {
    CPER_SEV_RECOVERABLE   = 0,
    CPER_SEV_FATAL         = 1,
    CPER_SEV_CORRECTED     = 2,
    CPER_SEV_INFORMATIONAL = 3,
};

// CXL agent type (UEFI 2.10 N.2.13 / cxl/event.h).
enum CxlCperAgentType : std::uint8_t {
    CPER_AGENT_RCD    = 0,
    CPER_AGENT_RCH_DP = 1,
    CPER_AGENT_DEVICE = 2,
    CPER_AGENT_LD     = 3,
    CPER_AGENT_FMLD   = 4,
    CPER_AGENT_RP     = 5,
    CPER_AGENT_DSP    = 6,
    CPER_AGENT_USP    = 7,
};

// cxl_cper_sec_prot_err.valid_bits.
enum : std::uint64_t {
    PROT_ERR_VALID_AGENT_TYPE   = 1ull << 0,
    PROT_ERR_VALID_AGENT_ADDR   = 1ull << 1,
    PROT_ERR_VALID_DEVICE_ID    = 1ull << 2,
    PROT_ERR_VALID_SERIAL_NUM   = 1ull << 3,
    PROT_ERR_VALID_CAPABILITY   = 1ull << 4,
    PROT_ERR_VALID_DVSEC        = 1ull << 5,
    PROT_ERR_VALID_ERROR_LOG    = 1ull << 6,
};

// UEFI GUID in wire byte order (mixed-endian D1/D2/D3, then D4[8]).
struct CxlCperGuid {
    std::uint8_t b[16] = {};
};

CxlCperGuid cxl_cper_guid_prot_err();
CxlCperGuid cxl_cper_guid_notify_cpe();
CxlCperGuid cxl_cper_guid_gen_media();
CxlCperGuid cxl_cper_guid_dram();
CxlCperGuid cxl_cper_guid_mem_module();
CxlCperGuid cxl_cper_guid_pcie();
bool cxl_cper_guid_eq(const CxlCperGuid& a, const CxlCperGuid& b);

// Serialized structures below are byte-packed: they are copied verbatim into
// the CPER body (no host padding allowed).
#pragma pack(push, 1)

// CXL RAS Capability Structure (CXL 3.0 8.2.4.16): 88 bytes.  This is the
// error log that follows the prot_err body when ERR_LOG is valid.
struct CxlCperRasRegs {
    std::uint32_t uncor_status = 0;
    std::uint32_t uncor_mask = 0;
    std::uint32_t uncor_severity = 0;
    std::uint32_t cor_status = 0;
    std::uint32_t cor_mask = 0;
    std::uint32_t cap_control = 0;
    std::uint32_t header_log[16] = {};
};

// cxl_cper_sec_prot_err (116 bytes).
struct CxlCperProtErr {
    std::uint64_t valid_bits = 0;
    std::uint8_t agent_type = CPER_AGENT_DSP;
    std::uint8_t reserved[7] = {};
    union {
        std::uint64_t rcrb_base_addr;
        struct {
            std::uint8_t function;
            std::uint8_t device;
            std::uint8_t bus;
            std::uint16_t segment;
            std::uint8_t reserved[3];
        } sbdf;
    } agent_addr = {};
    struct {
        std::uint16_t vendor_id = 0;
        std::uint16_t device_id = 0;
        std::uint16_t subsystem_vendor_id = 0;
        std::uint16_t subsystem_id = 0;
        std::uint8_t class_code[2] = {};
        std::uint16_t slot = 0;
        std::uint8_t reserved[4] = {};
    } device_id = {};
    struct {
        std::uint32_t lower_dw = 0;
        std::uint32_t upper_dw = 0;
    } dev_serial_num = {};
    std::uint8_t capability[60] = {};
    std::uint16_t dvsec_len = 0;
    std::uint16_t err_len = 0;
    std::uint8_t reserved_2[4] = {};
};

#pragma pack(pop)

// Parsed view.  The prot_err body is copied; dvsec/error-log bytes are copied
// so the caller may free the input buffer.
struct CxlCperProtErrView {
    std::uint32_t record_length = 0;
    std::uint32_t severity = 0;
    std::uint64_t timestamp = 0;
    std::uint64_t record_id = 0;
    CxlCperGuid notification_type;
    std::uint32_t section_offset = 0;
    std::uint32_t section_length = 0;
    std::uint32_t section_flags = 0;
    CxlCperProtErr prot;
    std::vector<std::uint8_t> dvsec;
    bool has_error_log = false;
    CxlCperRasRegs ras;
};

// CXL event-record CPER section (CXL CM/event records).  The section type is
// the event-record UUID; the body is cxl_cper_event_rec: a 32-byte CXL event
// header plus the 112-byte event record (the UUID is not repeated inside the
// body).  Model event records serialize to UUID[16] + event[112], so
// serialize().data()+16 is the event payload here.
enum : std::uint32_t {
    CPER_CXL_DEVICE_ID_VALID      = 1u << 0,
    CPER_CXL_DEVICE_SN_VALID      = 1u << 1,
    CPER_CXL_COMP_EVENT_LOG_VALID = 1u << 2,
};

struct CxlCperEventDevId {
    std::uint16_t vendor_id = 0;
    std::uint16_t device_id = 0;
    std::uint8_t func_num = 0;
    std::uint8_t device_num = 0;
    std::uint8_t bus_num = 0;
    std::uint16_t segment_num = 0;
    std::uint16_t slot_num = 0; // bits 2:0 reserved
    std::uint8_t reserved = 0;
};

struct CxlCperEventView {
    std::uint32_t record_length = 0;
    std::uint32_t severity = 0;
    std::uint64_t timestamp = 0;
    std::uint64_t record_id = 0;
    CxlCperGuid section_type;
    std::uint32_t section_offset = 0;
    std::uint32_t section_length = 0;
    CxlCperEventDevId device_id;
    std::uint64_t validation_bits = 0;
    std::uint64_t dev_serial = 0;
    std::vector<std::uint8_t> event; // 112-byte event record body
};

constexpr std::size_t kCxlCperEventBodySize = 112;

bool cxl_cper_build_event(const CxlCperGuid& section_type,
                          const CxlCperEventDevId& dev,
                          std::uint32_t validation_bits,
                          std::uint64_t dev_serial,
                          const std::vector<std::uint8_t>& event,
                          std::uint32_t severity,
                          std::uint64_t timestamp,
                          std::uint64_t record_id,
                          std::vector<std::uint8_t>* out,
                          std::string* err = nullptr);

bool cxl_cper_parse_event(const std::uint8_t* data, std::size_t n,
                          CxlCperEventView* out, std::string* err = nullptr);

// -----------------------------------------------------------------------------
// PCIe Error Section (UEFI 2.7 N.2.7; Linux cper_sec_pcie).  cp_sec_pcie is
// 208 bytes: 8+4+4+4+16+8+4+60+96.  The AER info block is the first 96 bytes
// of the AER capability register image (status/mask/severity/header log).
// -----------------------------------------------------------------------------
enum : std::uint64_t {
    CPER_PCIE_VALID_PORT_TYPE            = 0x0001,
    CPER_PCIE_VALID_VERSION              = 0x0002,
    CPER_PCIE_VALID_COMMAND_STATUS       = 0x0004,
    CPER_PCIE_VALID_DEVICE_ID            = 0x0008,
    CPER_PCIE_VALID_SERIAL_NUMBER        = 0x0010,
    CPER_PCIE_VALID_BRIDGE_CONTROL_STATUS= 0x0020,
    CPER_PCIE_VALID_CAPABILITY           = 0x0040,
    CPER_PCIE_VALID_AER_INFO             = 0x0080,
};

#pragma pack(push, 1)

struct CxlCperAerInfo {
    std::uint32_t header = 0;
    std::uint32_t uncor_status = 0, uncor_mask = 0, uncor_severity = 0;
    std::uint32_t cor_status = 0, cor_mask = 0, cap_control = 0;
    std::uint32_t header_log[4] = {};
    std::uint32_t root_command = 0, root_status = 0;
    std::uint16_t cor_err_source = 0, uncor_err_source = 0;
    std::uint8_t reserved[40] = {};
};

struct CxlCperPcie {
    std::uint64_t validation_bits = 0;
    std::uint32_t port_type = 0;
    struct { std::uint8_t minor = 0, major = 0, reserved[2] = {}; } version;
    std::uint16_t command = 0, status = 0;
    std::uint32_t reserved = 0;
    struct {
        std::uint16_t vendor_id = 0, device_id = 0;
        std::uint8_t class_code[3] = {};
        std::uint8_t function = 0, device = 0;
        std::uint16_t segment = 0;
        std::uint8_t bus = 0, secondary_bus = 0;
        std::uint16_t slot = 0;
        std::uint8_t reserved = 0;
    } device_id;
    struct { std::uint32_t lower = 0, upper = 0; } serial_number;
    struct { std::uint16_t secondary_status = 0, control = 0; } bridge;
    std::uint8_t capability[60] = {};
    CxlCperAerInfo aer_info;
};

#pragma pack(pop)

struct CxlCperPcieView {
    std::uint32_t record_length = 0;
    std::uint32_t severity = 0;
    std::uint64_t timestamp = 0;
    std::uint64_t record_id = 0;
    std::uint32_t section_offset = 0;
    std::uint32_t section_length = 0;
    CxlCperPcie pcie;
};

bool cxl_cper_build_pcie(const CxlCperPcie& pcie, std::uint32_t severity,
                         std::uint64_t timestamp, std::uint64_t record_id,
                         std::vector<std::uint8_t>* out,
                         std::string* err = nullptr);
bool cxl_cper_parse_pcie(const std::uint8_t* data, std::size_t n,
                         CxlCperPcieView* out, std::string* err = nullptr);

// Build one CPER record with a single CXL Protocol Error Section.
//   - dvsec bytes are appended verbatim after the 116-byte prot_err body;
//   - when ras != nullptr, the 88-byte CXL RAS capability block is appended
//     after DVSEC and ERR_LOG is marked valid;
//   - DVSEC/ERR_LOG valid bits are derived from the supplied data (other bits
//     are caller-owned).
// Returns false with an explicit error on size overflow; never truncates.
bool cxl_cper_build_prot_err(const CxlCperProtErr& prot,
                             const std::vector<std::uint8_t>& dvsec,
                             const CxlCperRasRegs* ras,
                             std::uint32_t severity,
                             std::uint64_t timestamp,
                             std::uint64_t record_id,
                             std::vector<std::uint8_t>* out,
                             std::string* err = nullptr);

// Parse and validate one record.  Rejects (returns false) on:
//   - buffer shorter than the record header + section descriptor;
//   - bad "CPER" signature, signature_end, revision or record_length;
//   - section offset/length outside the record;
//   - section type not the CXL Protocol Error GUID;
//   - prot_err body shorter than 116 bytes or DVSEC/error-log lengths that
//     overrun the section.
bool cxl_cper_parse_prot_err(const std::uint8_t* data, std::size_t n,
                             CxlCperProtErrView* out,
                             std::string* err = nullptr);

} // namespace cxl_tlm
