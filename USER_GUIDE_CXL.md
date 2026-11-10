# CXL Root + Switch Model -- User Guide (model side)

> Scope: CXL root, switch upper half (USP/crossbar), port adapters, and the
> no-OS Linux behavior proxy.  The customer `swdn` and EP are downstream DUTs.
> This page explains usage; scope and not-supported items are in
> `NOT_SUPPORTED_CXL.md`.

---

## 1. Layout

| Path | Contents |
|---|---|
| `include/cxl_*.h` | Public model headers |
| `lib/libcxl_switch_model.a` | Delivery library; no SystemC dependency |
| `console/` | Text console (`discover`, `run all`, `tc`, `perf`, ...) |
| `rtl_port/` | Grouped port interface and tie-offs (the only RTL) |
| `tb/uvm_ioctl/` | UVM TB + ioctl-style test selector (porting entry point) |
| `cosim/cxl/axi/` | 128-bit AXI4 host-DDR DPI bridge + external-SRAM bench |
| `smoke/` | Independent-compile smoke against `include/` + `lib/` |
| `tools/` | DUT docking-vector and bring-up-force generators |
| `dts/` | DTS profile template |
| `evidence/VALIDATION.md` | Validation summary |

---

## 2. Build and self-check

The released library and public headers are prebuilt.  Independent checks:

```sh
# Independent compile smoke (include/ + lib/ only)
make -C smoke run

# Console (prebuilt library)
make -C console
./console/cxl_switch_console --run-all
./console/cxl_switch_console --tc all
./console/cxl_switch_console --tc-cover
```

The released library and public headers are prebuilt; the checks below use
only `include/` and `lib/`.

### 2.1 Start here for porting: `tb/uvm_ioctl/`

The released tree contains a small UVM testbench with an ioctl-style test
selector.  It is the recommended integration starting point: the UVM TB polls a
Unix-domain socket (`/tmp/cxl_uvm_ctl.sock`) and dispatches testcases through a
DPI-C shim to the model library.  The transport and UVM loop are independent of
the test bodies; keep them and replace the dispatcher with customer sequences.

```sh
cd tb/uvm_ioctl
sh run.sh build          # control .so + CLI + UVM simv
sh run.sh sim            # start the UVM TB
sh run.sh list           # testcase ids/names
sh run.sh run all 1      # run all registered cases once
sh run.sh coverage
sh run.sh quit
```

Set `VCS` to the simulator command if it is not `vcs`; `SIM_ARGS` carries
simulator-specific options (for VCS, `SIM_ARGS=-no_save` is convenient).  See
`tb/uvm_ioctl/README.md` for the porting checklist and the boundary: the
example is a control/orchestration shell, not an RTL/VIP test pass.  No
downstream-port or endpoint RTL is shipped in the model repository.

The customer RTL tree can be supplied through `CXL_CFG_ROOT` (and
`CXL3SW_HOME` for DUT docking).  If the tree is absent, the checked-in
generated headers are used and the fallback is logged.

### 2.2 Optional 128-bit AXI4 host-DDR port

The model's host memory can be backed by an external SRAM that plays the role
of motherboard DDR.  The model is the AXI4 **master**; the SRAM is outside the
model.

```text
CxlRoot / CxlCacheHome --CxlAxiMaster--> CxlAxiBackend --> AXI4 pins --> SRAM
```

Wire contract: 128-bit data, 64-bit address, 4-bit AxID, 8 outstanding, AXI4
INCR, up to 256 beats (4 KiB) per burst.  Unaligned transfers are aligned down
to 16 bytes and selected with byte strobes.

Model API (`include/cxl_axi_master.h`):

```cpp
CxlAxiSramBackend sram;                 // functional backend, or a DPI backend
sram.init(0, 64ull << 20);
CxlAxiMaster axi;
axi.init(&sram);
axi.write(0x1000, src, 64);
axi.read (0x1000, dst, 64);

CxlRoot root;
root.init(4);
root.attach_axi(&axi);                  // root host memory now crosses AXI

CxlCacheSystem cache;
cache.init(CxlFlitMode::B68);
cache.attach_axi(&axi);                 // EP dirty evict -> AXI write;
                                        // home read -> AXI read -> H2D Data
```

`TB-CXL-AXI` covers aligned/unaligned/masked transfers, 4 KiB bursts,
outstanding requests, DECERR outside the SRAM window, root host memory, and the
CXL.cache EP-to-DDR path.  The optional pin-level DPI bench is in
`cosim/cxl/axi/` (`sh run.sh`, xsim; `SIM=vcs` selects VCS).  It instantiates
a 1 MiB reference `axi_ram` and checks the SRAM contents after the model
self-test.  The released tree links the bench against
`lib/libcxl_switch_model.a`.

The AXI4 port is an **optional host-memory attachment**, not a replacement for
the CXL/PCIe protocol boundary.  The external SRAM still belongs to the
platform/testbench; customer `swdn`/EP RTL is not part of the model.

---

## 3. Model layers

| Layer | Classes/files | Purpose |
|---|---|---|
| Contract | `cxl_core_cfg`, `cxl_flit`, `cxl_msg`, `cxl_pth`, `cxl_channel` | Parameters, flits, messages, PTH, credit/init |
| Registers | `cxl_reg_map`, `cxl_cfg_space`, `cxl_hdm`, `cxl_mailbox` | Address map, DVSEC/RegLoc/BAR, HDM, CCI |
| Topology | `cxl_root`, `cxl_usp`, `cxl_swdn_port`, `cxl_switch`, `cxl_hdm_prog` | Root/USP/DSP/crossbar/HDM programming |
| Adapter | `cxl_port_adapter` | TLM stub or RTL swdn backend (RTL backend requires the customer bridge) |
| Data path | `cxl_mem` | M2S/S2M, BE, poison/NXM, Tag, 68B/256B |
| Host DDR | `cxl_axi_master` | Optional 128-bit AXI4 master, external-SRAM backend |
| Linux proxy | `cxl_commands`, `cxl_linux` | P0/P1 commands, events, full sequence, console |
| PBR | `cxl_pbr`, `cxl_pbr_cci`, `cxl_cascade` | PTH/FAST/LDST, Direct P2P, two-level cascade, switch CCI |
| P1 cache/EMD | `cxl_cache`, `cxl_emd_bi` | Type-2 MESI cache, EMD/BISnp/BIRsp/HDM-DB |
| P2 fabric | `cxl_fabric`, `cxl_mld` | Multi-level PBR/ISL/G-FAM, MLD/FMLD/vCS |
| P3/P4 | `cxl_uio`, `cxl_fabric`, `cxl_mld`, `cxl_cache` | UIO/GMV/VTV/GIM, RGT modes, vPPB power, CQID OOO, directory limits |
| Validation | `cxl_tc`, `cxl_ioctl`, `cxl_trace`, `cxl_perf` | Testcase channel, ioctl ABI, MODEL_TRACE, performance windows |
| DTS | `cxl_dts` | Runtime template: ports, HDM/BAR bases, mailbox payload, flit mode |

`CxlPortAdapter::select_backend(TLM_STUB)` is available.  `RTL_SWDN` refuses to
run until the customer DPI bridge exists; it never pretends to pass.

---

## 4. Validation interfaces

### 4.1 Testcase channel (`cxl_tc_ref.h`, magic `'t'`)

Independent of the customer DUT UAPI:

```c
ioctl(fd, CXL_TC_GET_COUNT, &n);
ioctl(fd, CXL_TC_GET_INFO,  &info);   /* info.id = i */
ioctl(fd, CXL_TC_RUN,       &run);    /* run.id = i or CXL_TC_ID_ALL */
ioctl(fd, CXL_TC_COVERAGE,  &cov);
```

`run.iterations` replays a case (T6).  `CXL_TC_RUN_ALL` is an alias for
`RUN` with `CXL_TC_ID_ALL`.  The result struct reports `ran/passed/failed/rc`
and the first failure.  The model-side entry `cxl_tc_ioctl()` implements the
same ABI without an OS dependency.

### 4.2 Control ioctl (`cxl_ioctl_ref.h`, magic `'C'`)

`GET_INFO`, `ENUM`, `CFG_RD`, `CFG_WR`, `GET_STATE`, `GET_PERF`, `RESET_PERF`.
Unknown requests return `-ENOTTY`; out-of-range or malformed requests return
`-EINVAL` and are counted/logged, never silently accepted.

### 4.3 Tracing (`MODEL_TRACE`)

Set `MODEL_TRACE=1` (or use console `trace on`).  Six classes are emitted:

```
[TRACE][STAGE]   sequence/testcase stage
[TRACE][IRQ]     interrupt or event delivery
[TRACE][EXC]     unexpected protocol/model state
[TRACE][ERR]     explicit error (never silent)
[TRACE][RESULT]  per-run result
[TRACE][SUM]     summary
```

Tracing is observational; it does not change model behavior.

### 4.4 Coverage

`CXL_TC_COVERAGE` / console `tc cover` prints tested cases with pass counts,
registered-but-untested cases, and explicit untestable items with reasons
(customer DUT co-simulation, RTL PBR profile, PHY-level behavior, etc.).

### 4.5 DTS

`dts/onetest-cxl.dts` is the factory template.  Supported properties:

```
compatible = "onetest,cxl-switch";
onetest,num-ports = <1..32>;
onetest,hdm-base = <address>;
onetest,bar0-base = <address>;
onetest,mailbox-payload = <256|512|1024>;
onetest,flit-mode = "68b" | "256b" | "pbr";
```

`cxl_model_load_dts()` parses it; `CxlLinuxProxy::init(dts)` applies ports,
flit mode, and logs the profile.  Malformed or incompatible templates are
explicit errors.

---

## 5. Console

| Command | Description |
|---|---|
| `discover` | Enumerate the CXL device tree |
| `ls` | Print the enumerated tree |
| `hdm` | Reprogram/verify HDM decoders |
| `region` | Create volatile regions from Identify capacity |
| `mbox <port>` | Identify + Health |
| `read <port> <off>` | Read 64 bytes |
| `write <port> <off> <hex,...>` | Write comma-separated bytes |
| `event <port>` | Event Get/Clear round-trip |
| `poison <port>` | Query the poison list |
| `perf` | Print CXL.mem and configuration performance windows |
| `trace on\|off\|reset` | Control MODEL_TRACE / counters |
| `cover` | Proxy coverage summary |
| `run all` | Full Linux proxy sequence |
| `tc list` | List testcases |
| `tc run <id\|all> [n]` | Run one case or all, optional iterations |
| `tc cover` | Testcase coverage report |

Batch: `--dts <path>`, `--run-all`, `--cover`, `--tc <id|all> [n]`,
`--tc-list`, `--tc-cover`.

---

## 6. Covered and not covered

**Current model-side baseline (2026-11-11)**: the last complete tree
regression is **115/115 gates PASS, 0 FAIL**; the current tree has
**122 gates**, with R7-R14 directed PASS but no full 122-gate rerun.
43-bench capability suite PASS; delivery library
**52 objects / 86 CXL public headers**.  A local integration of four `swdn`
RTL cores + four RTL EPs passes MC/BC/MemRd M1-M5 (14/14).  The
customer-environment co-simulation remains SKIP.

Covered: contract/bit-level messages, DVSEC/RegLoc/HDM/mailbox, enumeration,
HDM programming, CXL.mem (BE/poison/NXM/Tag), Linux proxy sequence, events,
PBR/PTH/FAST/LDST, Direct P2P, two-level cascade, N=1..32, port adapter RTL/DPI
stub/library/console; Type-2 MESI cache, LSA/partition, CEL/FW/poison/background,
PBR CCI, 2/4/8-way interleave, EMD/BISnp/HDM-DB, multi-outstanding, multi-level
PBR/ISL/G-FAM, MLD/FMLD/vCS; multi-device cache directory, multi-host HDM-DB,
RGT 256/advanced modes, UIO/GMV/VTV/GIM, vPPB Type1/hot-plug/capabilities/power,
PBR CCI remainder, EMD 1..32/EMDErr, 3/6/12-way device decoders, cache CQID OOO,
HDM-DB dirty lines, GIM groups, directory limits; vPPB extension chain
(DPC/LTR/PTM/VC/L1SS/DLF/DSN/ReBAR/MFVC/DOE/DVSEC) and standard ReBAR layout
on the PCIe, Type-3 and vPPB faces; DTS, testcase channel, ioctl ABI,
MODEL_TRACE, perf windows, independent library smoke.

Not covered / explicit boundaries:

- Full PCIe extended capability surface inside vPPB: the R4 chain adds DPC,
  LTR, PTM, VC, L1SS, DLF, DSN, ReBAR, MFVC, DOE and DVSEC, but remaining
  extended-capability registers still read 0.  MFVC/DOE/DVSEC offsets are
  model-defined pending the customer's formal vPPB layout.
- Vendor-specific slot power timing is a configurable per-rail table; real
  electrical rails are not modeled.
- GIM internal address translation/GFD routing beyond the configurable
  selector mapping is implementation-defined.
- Vendor-specific advanced-congestion metric (the congestion input and
  secondary penalty are model-level approximations).
- CXL.cache TEE, vendor-specific multi-level replacement policies, long
  multi-host soak tests.
- Dynamic Capacity (DCD) is opt-in and off by default; multi-host/tag
  sharing is implemented (1..8 hosts), but the model does not own a media
  array or perform scans (owner hooks connect a device backend).
- Security Send/Receive implements the CXL transport envelope (SECP/SPSP/
  reserved bytes/protocol data) with a protocol-handler registry and matching
  response queue; TCG/SED payload formats remain owner-supplied, and the
  feature is off by default.  IDE key/stream/IV management and AES-256-GCM
  are modeled (IDE_KM and IDE crypto benches pass); real RTL link-layer IDE
  framing/TSP is owned by the customer link-layer IP.  DOE supports
  Discovery, CDAT, CMA/Secured CMA and an optional IDE_KM object;
  unrecognized objects are rejected.
- Mailbox opcodes outside the implemented whitelist return `Unsupported`
  (for example 0x4900+, 0x5717+).  0x0106 Event Notification is modeled as
  the component-originated MCTP CCI payload/queue.  0x0107/0x0108 are
  GFD-originated enhanced-event VDM payloads (encode/decode helpers), and
  0x0109/0x010A are GAE-only Get/Set GAM Buffer helpers with a 32-byte-entry
  circular buffer; they are not Type-3 host mailbox commands and the
  GFD/GAE transport is platform-owned.
- CPMU provides counter/register blocks and an optional bridge to an MSI-X
  table/root IRQ; platform-specific 64-KB RegLoc block mapping remains an
  owner responsibility.
- VPD is a register window.  The EP/PCIe base model synthesizes a standard
  expansion-ROM image when a ROM window is configured (signature, x86 entry,
  PCIR identity and checksum); an owner image can be installed instead.  The
  vPPB ROM BAR is register-only and the CXL component model does not
  advertise a ROM BAR (both are optional PCIe choices).
- LTR/VDM/PTM multi-DW payloads are supported by the C++ model; the W5 VIP
  host agent carries up to 32 bytes in one 256-bit beat (a directed 2-DW VDM
  was accepted; LTR/PTM are one-DW by definition).
- Customer `swdn` + EP DUT RTL co-simulation (M6.2-M6.7): requires the customer
  environment, EP peer, and full DPI bridge.  The model-side docking vectors,
  port check, force subsets, and wrapper are complete; the local four-port
  swdn + RTL-EP integration passes M1-M5 (14/14).
- Customer RTL PBR/256B profile: the delivered CTL configuration has PBR
  disabled; the model spec profile is validated, the RTL profile is SKIP.
- PHY electrical/link training/CRC/retry below the CTL app interface.
- Physical media poison injection; the model maintains the poison list only.
- NTB and DM are minimal model-defined scope (F1a/F1b), not full vendor APIs.

The model reports unimplemented cases as `Unsupported` or `SKIP`; a skip is
never counted as a pass.
