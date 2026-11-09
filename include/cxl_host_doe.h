// -----------------------------------------------------------------------------
// cxl_host_doe.h -- host-side PCIe DOE (Data Object Exchange) discovery.
//
//   Linux discovers DOE mailboxes before using IDE/SPDM.  This helper performs
//   the mandatory PCI-SIG Discovery data object over the DOE mailbox and parses
//   the response.  It is transport-agnostic (configuration-space read/write
//   callbacks) and bounded: a busy/error/timeout is never reported as success.
//
//   DOE Extended Capability registers (relative to the capability):
//     +04 Capability  (RO)  [11:0] Number of DWs
//     +08 Control     (RW)  bit0 DOE Enable, bit1 DOE Abort
//     +0C Status      (RO)  bit0 DOE Busy, bit1 Interrupt, bit2 DOE Error
//     +10 Write Data Mailbox (one DW per write)
//     +14 Read Data Mailbox  (one DW per read)
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cxl_tlm {

struct CxlHostDoeIo {
    std::function<bool(std::uint16_t off, unsigned size, std::uint32_t* v)> rd;
    std::function<bool(std::uint16_t off, unsigned size, std::uint32_t v)> wr;
    std::function<void(const std::string&)> log;
    unsigned timeout_polls = 10000;
};

struct CxlHostDoeInfo {
    std::uint16_t vendor = 0;
    std::uint8_t  data_object_type = 0;
    std::uint8_t  length_dws = 0;
    std::uint32_t response_dw0 = 0;
    std::uint32_t response_dw1 = 0;
    int polls = 0;
};

// Perform DOE Discovery (data object type 0x01, vendor 0x0001).  Returns false
// on transport failure, DOE error, malformed/short response, or timeout.
bool cxl_host_doe_discover(const CxlHostDoeIo& io, std::uint16_t cap_off,
                           CxlHostDoeInfo* out, std::string* err = nullptr);

// -----------------------------------------------------------------------------
// CDAT (Coherent Device Attribute Table) table-access DOE (vendor 0x1E98,
// protocol 0x02).  Layouts follow Linux 6.18 drivers/cxl/cxlpci.h and
// include/acpi/actbl1.h.  The host reads the 16-byte CDAT header and every
// entry, validates lengths and reports the checksum (sum of all bytes == 0).
// -----------------------------------------------------------------------------
struct CxlHostDoeCdat {
    std::vector<std::uint8_t> table; // header + entries actually read
    std::uint32_t length = 0;        // actual bytes read (header length may be
                                     // larger; adjusted like the Linux driver)
    bool checksum_ok = false;
    unsigned entries = 0;
};

// Model-side table construction helper (used by the device/CXL component and
// by unit tests).  append_dsmas() appends one 24-byte DSMAS entry to an
// entries buffer; finalize() prepends the 16-byte header, fills the length and
// sequence fields, and computes the checksum.  entries.size() must be a
// multiple of 4 (CDAT entries are dword-sized).
std::size_t cxl_cdat_append_dsmas(std::vector<std::uint8_t>* entries,
                                  std::uint8_t handle,
                                  std::uint64_t dpa_base,
                                  std::uint64_t dpa_length,
                                  std::uint8_t flags);
std::vector<std::uint8_t> cxl_cdat_finalize(
    const std::vector<std::uint8_t>& entries, std::uint32_t sequence);

// Read the full CDAT table through the DOE table-access protocol.  Returns
// false on transport/DOE/malformed-length errors; a bad checksum is reported
// via out->checksum_ok (false) and a log line, not a silent success.
bool cxl_host_doe_read_cdat(const CxlHostDoeIo& io, std::uint16_t cap_off,
                            CxlHostDoeCdat* out, std::string* err = nullptr);

} // namespace cxl_tlm
