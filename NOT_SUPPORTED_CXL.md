# CXL Model -- Scope, IP Ledger, and Not-Supported List

This file is the customer-facing boundary statement.  A `SKIP` or
`Unsupported` result is never counted as a pass.

**Current model-side baseline (2026-11-11)**: the last complete tree
regression is **115/115 gates PASS, 0 FAIL**; the current tree has
**129 gates**, with R7-R21 directed PASS but no full 129-gate rerun;
43-bench capability suite PASS; delivery library
**53 objects / 89 CXL public headers**; the `tlmer/cxl4.0switch` archive
was verified by an independent staged smoke/console.
A local integration of four `swdn` RTL cores + four RTL EPs passes M1-M5
(14/14); the customer-environment co-simulation (item 13) remains SKIP.

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
2. **PCIe extended capability surface inside vPPB (expanded in R4/R6).**
   The vPPB exposes AER (status/mask/severity/ECRC/header log/root status),
   ATS (enable/STU + invalidation counters), ACS (control + P2P redirect
   check) and SR-IOV (VF count/offset/stride/enable) in addition to
   PM/PCIe/Slot/Link/MSI.  R4 adds DPC, LTR, PTM, VC, L1SS, DLF, DSN, ReBAR,
   MFVC, DOE and DVSEC; R6 adds Power Budgeting, ARI, PRI, TPH, PASID,
   Secondary PCIe, NPEM, VF ReBAR and PL16/32/64 (standard register faces at
   0x248..0x327).  Any still-unmodeled higher offset reads zero.  The
   data-plane behavior of these features remains with the owner; the
   registers are a capability surface.
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
7. **CXL.cache long multi-host soak/stress.**  The functional MESI model
   covers I/S/E/M, snoop, eviction, multi-device directory, CQID OOO and LRU.
   A model-level TEE trust boundary is implemented (secure 64-byte ranges,
   TEE/non-TEE access isolation, per-line TEE tags, and owner
   protect/integrity hooks at the home-memory boundary); the model ships no
   production cipher or key policy, and vendor algorithms plus long-duration
   multi-host soak are not shipped.  Device-level and directory-level
   replacement hooks are provided for owner/vendor policies (the model ships
   LRU and a 20k-operation functional coherence soak).
8. **CXL.io PTH/PBR.**  The customer RTL lacks the relevant CXL.io PBR fields;
   the model's CXL.io PTH behavior is a model-level interpretation and the RTL
   profile skips it.
9. **CCI opcodes outside the implemented set** (for example 5717h and later)
   return `Unsupported`.  0107h/0108h are modeled as GFD-originated
   enhanced-event VDM payloads, and 0109h/010Ah as GAE GAM Buffer command/ring
   helpers; they are GFD/GAE-only and are not Type-3 host mailbox commands.
   4900h+ is GFD-only.  Event Notification 0106h is modeled as the
   component-originated MCTP CCI payload/queue, not a host mailbox command.
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
    bridge.  A local integration in this tree (four `swdn` RTL cores + four
    RTL EPs) passes M1-M5 (14/14); it does not replace the
    customer-environment co-simulation.
14. **Customer RTL PBR/256B profile.**  The delivered CTL configuration has PBR
    disabled; the model implements the spec profile, while the RTL profile is
    SKIP until the customer enables PBR.
15. **Real Linux kernel callbacks.**  The Linux proxy reproduces the behavior
    sequence (ACPI/discovery/HDM/mailbox/memory/events/poison/PM); it is not a
    kernel module and does not claim kernel timing.
16. **Dynamic Capacity Device (DCD).**  DCD is opt-in and disabled by
    default (0x4800-0x4803 return `Unsupported`).  When enabled, the model
    covers configuration, extent list, add/release responses, the class-4
    Dynamic Capacity Event Log and Identify offset 0x43.  Multi-host/tag
    sharing is implemented (1..8 hosts, shared-tag accounting).  A reference
    media backend (`CxlDcdReferenceMedia`) provides sparse 64-byte extents,
    read/write/zero/sanitize, bad-block injection and scan primitives.  Type3
    Scan Media (0x4304/0x4305) consumes an owner `media_scan_range` hook,
    merges it with the poison fallback, and Media Operation (0x4402)
    zero/sanitize reaches the owner `media_zero_range`/`media_sanitize_range`
    hooks.  It is not a physical media implementation; the actual media and
    platform replacement policy remain owner-side.
17. **Security Send/Receive, IDE and DOE.**  Security Send/Receive implements
    the CXL transport envelope (SECP, SPSP, reserved bytes, protocol data) with
    a protocol-handler registry and a matching response queue; it is disabled
    by default (`Unsupported`).  `cxl_tcg_sed.h` supplies a reference TCG/SED
    handler (TCG framing, Level 0 Discovery, reference SED lock/passphrase/
    revert state machine); full Opal method-UID dispatch, production SSC
    semantics and key management remain owner-side (a raw
    `security_passthrough` hook is also retained).  IDE is modeled at the key/stream/IV and AES-256-GCM level (IDE_KM and
    IDE crypto benches pass) with an owner cipher hook; real RTL link-layer
    IDE framing and TSP integration are owned by the customer link-layer IP.
    DOE supports Discovery, CDAT table access, CMA / Secured CMA via an
    attached SPDM session (vendor 0x0001 type 0x01/0x02), and an optional
    IDE_KM object; unrecognized objects are explicitly rejected.
18. **CPMU.**  The model provides counter/register blocks (multi-counter,
    multi-instance, overflow interrupt) and an optional bridge to an MSI-X
    table/root IRQ.  Additional 64-KB RegLoc blocks can be registered through
    the owner API with alignment/overlap checks; mapping them into a specific
    platform's DVSEC layout remains an owner responsibility.
19. **VPD / ROM.**  The VPD register window is implemented, with
    owner-configurable identifier and VPD-R fields.  The EP/PCIe base model
    now synthesizes a standard expansion-ROM image (55AA, x86 entry, PCIR
    identity, checksum) when a ROM window is configured; an owner image can
    replace it.  The vPPB ROM BAR remains
    register-only and the CXL component model does not advertise a ROM BAR
    (both are optional PCIe choices).
20. **LTR/VDM/PTM multi-DW payloads.**  The C++ model supports arbitrary DW
    payloads.  The W5 VIP SV host agent now carries up to 32 bytes in one
    256-bit beat; a directed 2-DW VDM was accepted in the W5 run.  LTR and PTM
    messages are one-DW by definition.
21. **Mailbox command set.**  Opcodes outside the implemented whitelist
    return `Unsupported` with no silent success (for example 0x4900+
    GFD-only and 0x5717+).  0x0107/0x0108 are modeled as GFD-originated
    enhanced-event VDM payloads (encoding/decoding, reserved-bit and length
    checks), and 0x0109/0x010A as GAE GAM Buffer command/ring helpers
    (32-byte entries, 2..256 entries, overflow/wrap).  The model also composes
    a GAE endpoint with a bounded GFD 0108 VDM inbox; they are not Type-3 host
    mailbox commands, and the real fabric/VDM transport and host GAM-buffer
    memory remain platform-owned.
    DCD is opt-in and off by default; 0x4600/0x4601 are admitted only when
    the security envelope or the owner passthrough hook is enabled.

## Validation status semantics

- `TB_*: ALL PASS (n/n)` means every listed criterion passed.
- Red variants must fail with the documented signature (see `evidence/`).
- The last complete model-side tree run (2026-11-10) is **115/115 gates
  PASS** plus the 43-bench capability suite.  The current tree has **129
  gates**; R7-R21 were run directly and pass, but a full 129-gate rerun was
  not performed.  The released archive was verified by an independent
  staged smoke/console.
- DUT co-simulation and RTL-profile items remain `PENDING`/`SKIP` until the
  customer environment is available.

## Validation interfaces

- `--kernel-log` / `klog on`: kernel-style CXL raw prints for
  discovery/HDM/mailbox/memory/event/poison/PM, reproduced by the no-OS Linux
  proxy.  They are model prints in the same format as the kernel messages,
  not real `dmesg` output.
- Model-level validation covers every 68B CXL.mem/CXL.cache opcode with
  per-channel credits and tag/UQID/CQID checks; on the real SW + EP pair the
  run ends with `W6_SIM: PASS` (344 messages per side, `tag_err=0`).
