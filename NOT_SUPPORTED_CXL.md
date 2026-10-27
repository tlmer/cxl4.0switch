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

1. **NTB and DM** are out of scope by project decision; they are excluded from
   the CXL build and regression.
2. **Full PCIe extended capability surface inside vPPB** (AER/ATS/ACS/ECRC/
   SR-IOV/etc.).  Only the PM, PCIe, Slot, Link, and MSI capabilities needed by
   the vPPB model are implemented; unimplemented registers read zero.
3. **Vendor-specific slot power timing.**  Power sequencing is modeled with
   `power_ramp_ticks`; real rails and board timing are not modeled.
4. **GIM internal address translation/GFD mapping.**  CXL defines this as
   implementation-specific; the model uses GIM ID + offset.
5. **Full GMV/VTV register-table layout.**  The model implements the 4-Kb
   vectors, versions, FAST/GIM interleave, and authorization checks; every
   vendor-specific table field is not implemented.
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
