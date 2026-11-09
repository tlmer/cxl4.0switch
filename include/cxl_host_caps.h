// -----------------------------------------------------------------------------
// cxl_host_caps.h -- Linux-like host policy for PCIe capabilities.
//
//   The remote enumerator discovers the endpoint's configuration space; this
//   module applies the host-side policy Linux would apply: PCIe control bits
//   (MPS/MRRS/RO/ASPM/LTR/10-bit Tag/Atomic/OBFF), PM D0, AER masks/severity/
//   ECRC, ACS, ATS, PASID, DPC, ReBAR, SR-IOV, Slot/hotplug, VPD and MSI-X
//   table programming when the transport can write memory.
//
//   The module is transport-agnostic: callers provide read/write callbacks.
//   Capabilities that the endpoint does not expose are skipped (never faked).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_enum.h"

#include <cstdint>
#include <functional>
#include <string>

namespace cxl_tlm {

struct CxlHostCapIo {
    std::function<bool(std::uint16_t off, unsigned size, std::uint32_t* v)> rd;
    std::function<bool(std::uint16_t off, unsigned size, std::uint32_t v)> wr;
    // Optional: fill in when the transport supports CXL.io memory writes.
    // MSI-X table/PBA and VPD/ROM accesses use it; a null callback means
    // "configuration-only" and the action is reported as deferred.
    std::function<bool(std::uint64_t addr, const void* src, std::size_t n)> mem_write;
    // Optional CXL.io memory read; used to verify MSI-X table writes and the
    // expansion ROM signature.  A null callback leaves those steps deferred.
    std::function<bool(std::uint64_t addr, void* dst, std::size_t n)> mem_read;
    // Optional: register an MSI-X vector in the host root's IRQ table.
    std::function<void(std::uint8_t vector, std::uint64_t addr, std::uint32_t data)> enrol_irq;
    // Optional ARI function-indexed configuration access (fn8 = Device[4:0]
    // << 3 | Function[2:0]).  VF enumeration is opt-in because some DUT
    // transports ignore the function number and would alias the PF view.
    std::function<bool(std::uint8_t fn8, std::uint16_t off, unsigned size,
                       std::uint32_t* v)> vf_rd;
    std::function<bool(std::uint8_t fn8, std::uint16_t off, unsigned size,
                       std::uint32_t v)> vf_wr;
    bool enumerate_vfs = false;
    // Optional CXL HDM range to program in the Device DVSEC (256 MB aligned).
    // Zero size means "read/report only" (the default for a generic EP).
    std::uint64_t hdm_base = 0;
    std::uint64_t hdm_size = 0;
    // Linux leaves L1 PM Substates disabled unless the platform policy opts
    // in (L1.1/L1.2 require an upstream-port common-mode contract).  PTM
    // requester enable is per-device and safe to apply when advertised.
    bool enable_l1ss = false;
    bool enable_ptm = true;
    // Read back MSI-X table writes when mem_read exists.  A DUT whose memory
    // path does not answer MemRd can disable this to avoid a long timeout;
    // the log then says write-only, never "verified".
    bool verify_mem = true;
    std::function<void(const std::string&)> log;
};

// Apply Linux-like host configuration to the capabilities discovered on `d`.
// Safe to call for an endpoint or a switch port; absent capabilities are
// skipped.  Returns the number of capabilities that were programmed.
int cxl_host_apply_caps(const CxlEnumDev& d, const CxlHostCapIo& io);

// Find a standard/extended capability offset recorded by walk_caps().
std::uint16_t cxl_host_find_cap(const CxlEnumDev& d, std::uint16_t id, bool ext);

// Linux-like PM state transition (D0/D1/D2/D3hot).  Returns false when the
// device has no PM capability, the state is unsupported, or the transport
// fails.  The PME_En/PME_Status bits are preserved except for the state field.
bool cxl_host_set_power_state(const CxlEnumDev& d, const CxlHostCapIo& io,
                              unsigned state, std::string* err = nullptr);

// Slot / hotplug control (PCIe Slot Capability registers at `slot_off`:
// Capabilities +0x00, Control +0x04, Status +0x06).  The host drives the
// power controller and writes the command-completed RW1C bit, matching the
// Linux hotplug sequence.  Absent/failed accesses are reported, never faked.
struct CxlHostSlotStatus {
    bool attention = false;
    bool power_fault = false;
    bool mrl_open = false;
    bool presence = false;
    bool command_completed = false;
    bool dll_active = false;
};
bool cxl_host_slot_read_status(const CxlHostCapIo& io,
                               std::uint16_t slot_off,
                               CxlHostSlotStatus* st,
                               std::string* err = nullptr);
bool cxl_host_slot_power(const CxlHostCapIo& io, std::uint16_t slot_off,
                         bool on, unsigned max_polls = 1000,
                         std::string* err = nullptr);
bool cxl_host_slot_indicators(const CxlHostCapIo& io,
                              std::uint16_t slot_off,
                              unsigned attention, unsigned power,
                              std::string* err = nullptr);

// DPC (Downstream Port Containment) host control.  `dpc_off` points at the
// extended capability header (as recorded by walk_caps); Control is at +6
// (high half of the +4 dword) and Status/Source ID at +8/+10.
struct CxlHostDpcStatus {
    bool enabled = false;
    bool triggered = false;
    std::uint8_t reason = 0;
    std::uint16_t source_id = 0;
};
bool cxl_host_dpc_status(const CxlHostCapIo& io, std::uint16_t dpc_off,
                         CxlHostDpcStatus* st, std::string* err = nullptr);
bool cxl_host_dpc_trigger(const CxlHostCapIo& io, std::uint16_t dpc_off,
                          std::string* err = nullptr);
bool cxl_host_dpc_clear(const CxlHostCapIo& io, std::uint16_t dpc_off,
                        std::string* err = nullptr);

// Optional global IRQ registrar used by the host DPI.  It receives the
// requester ID plus the vector/address/data of an MSI-X table entry that was
// programmed by cxl_host_apply_caps().  The model root installs the callback;
// the local/unit-test transports leave it empty.
using CxlHostIrqFn = std::function<void(std::uint16_t reqid,
                                        std::uint8_t vector,
                                        std::uint64_t addr,
                                        std::uint32_t data)>;
void cxl_host_set_irq_callback(CxlHostIrqFn fn);
CxlHostIrqFn cxl_host_irq_callback();

// Optional CXL HDM range policy (Device DVSEC Range1).  The host DPI sets it
// from CXL_HOST_HDM_BASE/SIZE; a unit test can set it directly.  Zero size =
// report only, no range programming.
struct CxlHostHdmPolicy {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
};
void cxl_host_set_hdm_policy(std::uint64_t base, std::uint64_t size);
CxlHostHdmPolicy cxl_host_hdm_policy();

// Platform policy for L1 PM Substates.  Default false: L1.1/L1.2 require
// an upstream common-mode contract, so the host only enables them when the
// platform explicitly opts in.  PTM requester enable defaults to true.
void cxl_host_set_l1ss_enable(bool on);
bool cxl_host_l1ss_enable();

// Global opt-out for MSI-X table readback (used by a DUT without a CXL.io
// memory read path).  Default true: readback is attempted when available.
void cxl_host_set_verify_mem(bool on);
bool cxl_host_verify_mem();

// Opt-in ARI VF enumeration policy (default false: a transport that ignores
// the function number would alias every VF to the PF view).
void cxl_host_set_vf_enumeration(bool on);
bool cxl_host_vf_enumeration();

} // namespace cxl_tlm
