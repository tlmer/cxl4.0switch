# CXL Model -- Scope, IP Ledger, and Not-Supported List

This file is the customer-facing boundary statement.  A `SKIP` or
`Unsupported` result is never counted as a pass.

## Scope and IP ledger

| Layer | Owner | Notes |
|---|---|---|
| CXL root complex, switch upper half (USP, crossbar, DSP port adapters) | this model | SystemC/C++ functional model |
| No-OS Linux behavior proxy, mailbox/CCI, events, poison, HDM programming | this model | functional sequence, not a real kernel |
| Validation interfaces (DTS, testcase, ioctl, trace, coverage, perf, console) | this model | independent of the customer DUT UAPI |
| CTL `swdn_cxl` downstream switch port RTL | customer DUT | connected through the port adapter |
| Endpoint DUT behind `swdn` | customer DUT | enumerated/accessed by the model |
| Port adapter/tie-off/DPI stub | this model | the only RTL in this tree |
| PHY, LTSSM, link training, DLL retry, PCS | downstream CTL/PHY | not modeled |

## Not supported / explicit boundaries

1. **NTB and DM (implemented F1a/F1b, minimal model-defined scope).**  The
   CXL switch owns a minimal NTB function block (`cxl_ntb`: 4 symmetric
   translation windows, doorbell, 8x32-bit scratchpad, explicit
   out-of-window misses) and a minimal DM v1 endpoint (`cxl_dm`: identify,
   status, health, error log, warm/cold reset, FM-owned mode; unknown
   opcodes return Unsupported).  The DM command set is model-defined and not
   the full vendor management API.
2. **Full PCIe extended capability surface inside vPPB (implemented F2).**
   The vPPB now exposes AER (status/mask/severity/ECRC/header log/root
   status), ATS (enable/STU + invalidation counters), ACS (control + P2P
   redirect check) and SR-IOV (VF count/offset/stride/enable) at
   0x100/0x140/0x150/0x160, in addition to PM/PCIe/Slot/Link/MSI.  Remaining
   extended-capability registers still read zero.
3. **Vendor-specific slot power timing (implemented F3).**  Power sequencing
   now supports a configurable per-rail/board timing table (`add_power_rail`:
   delay + ramp + power-good per rail, plus a reset-release window) and
   exposes rail-good progress.  Real electrical rails are still not modeled;
   the timing table is the configurable board view.
4. **GIM internal address translation/GFD mapping (implemented F4).**  The
   model now supports a configurable GIM window translation table
   (`add_gim_map`: GIM ID + window + target GIM + target offset + GFD).  With
   no map the historical GIM ID + offset identity behavior is used.  The
   vendor-specific GFD routing semantics beyond the selector value remain
   implementation-defined.
5. **Full GMV/VTV register-table layout (implemented F5).**  The model now
   exposes the full 4-Kb GMV/VTV tables (512-byte vectors per VCS / per
   VCS+LD), versions, permission/interleave flag words, bit set/clear, slice
   read/write and out-of-range errors, in addition to the set-based
   authorization checks.  Vendor-specific flag-bit meanings are model-defined.
6. **Vendor advanced-congestion algorithm.**  The congestion metric is an input
   and `secondary_penalty` is a model approximation; the vendor algorithm is
   not reverse-engineered.
7. **CXL.cache TEE**, vendor-specific multi-level replacement policies, and
   long multi-host soak/stress.  The functional MESI model covers I/S/E/M,
   snoop, eviction, multi-device directory, CQID OOO, and LRU.
8. **CXL.io PTH/PBR.**  The customer RTL lacks the relevant CXL.io PBR fields;
   the model's CXL.io PTH behavior is a model-level interpretation and the RTL
   profile skips it.
9. **CCI opcodes outside the implemented set** (for example 5717h and later)
   return `Unsupported`.
10. **3/6/12-way on a switch.**  CXL 4.0 permits 3/6/12-way only for CXL.mem
    device decoders; switch/USP target lists have 1/2/4/8 entries.  The model
    rejects 3/6/12-way on a switch with `Error Not Committed`.  This is a
    spec-correct rejection, not a missing feature.
11. **PHY-level behavior** (electrical, link training, CRC, retry, equalization)
    is below the CTL application interface and is not modeled.
12. **Physical media poison injection.**  The model maintains the poison list
    and event/response semantics; there is no physical media path.
13. **Customer `swdn` + EP DUT RTL co-simulation (M6.2-M6.7).**  The model-side
    docking pieces are complete: exact wire vectors (20/20), port check
    (107/107), 94-force manifest, DUT wrapper, and generated bridge stubs.  A
    full VCS run requires the customer environment, EP peer, and complete DPI
    bridge.
14. **Customer RTL PBR/256B profile.**  The delivered CTL configuration has PBR
    disabled; the model implements the spec profile, while the RTL profile is
    SKIP until the customer enables PBR.
15. **Real Linux kernel callbacks.**  The Linux proxy reproduces the behavior
    sequence (ACPI/discovery/HDM/mailbox/memory/events/poison/PM); it is not a
    kernel module and does not claim kernel timing.

## Validation status semantics

- `TB_*: ALL PASS (n/n)` means every listed criterion passed.
- Red variants must fail with the documented signature (see `evidence/`).
- DUT co-simulation and RTL-profile items remain `PENDING`/`SKIP` until the
  customer environment is available.

## Validation interfaces

- `--kernel-log` / `klog on`: kernel-style CXL raw prints for
  discovery/HDM/mailbox/memory/event/poison/PM, reproduced by the no-OS Linux
  proxy.  They are model prints in the same format as the kernel messages,
  not real `dmesg` output.
- `cxl_stimu_ex` / `ep_stimu_ex` are local stimulus examples (not published).
  They drive every 68B CXL.mem/CXL.cache opcode with per-channel credits and
  tag/UQID/CQID checks; on the real SW + EP pair the run ends with
  `W6_SIM: PASS` (344 messages per side, `tag_err=0`; evidence
  `evidence/M6/W6_real_sw_ep/`).
