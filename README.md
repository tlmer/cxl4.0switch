# CXL switch Model

Repository: <https://github.com/tlmer/cxl4.0switch>

A functional C++17 model of a **CXL 4.0 root complex, switch upper half
(USP/crossbar/downstream-port adapters), and a no-OS Linux behavior proxy**.
The customer connects one or more downstream-port and endpoint DUTs behind it;
this repository provides **the host-side model only**: the port-adapter RTL,
binary library, public headers, console, DTS template, UVM control example, and
validation artifacts.  It does not ship downstream-port (`swdn`) or endpoint
RTL.

## Highlights

- CXL.io configuration/MMIO/events; CXL.mem 68B/256B/PBR, HDM, BE, poison/NXM
- Complete PCIe configuration base (Type0/Type1, BAR sizing/MSE, standard and
  extended capability chains) and endpoint/cascaded-switch enumeration
  (model-side; the endpoint/cascade HDM path is verified through the model)
- CXL.cache Type-2 MESI, multi-device directory, CQID out-of-order, LRU limits
- PBR: PTH, SPID/DPID, FAST/LDST/IDT, Direct P2P, multi-level fabric/ISL/G-FAM
- PBR switch CCI, RGT 256 / Table 7-87 mix and advanced congestion avoidance
- MLD/FMLD/vCS, vPPB Type-1 configuration, hot-plug/power sequencing
- EMD/BISnp/BIRsp/HDM-DB, multi-host dirty lines
- UIO Direct P2P, GMV/VTV/GIM, FAST/GIM interleave
- Linux proxy: ACPI/CEDT view, discovery, HDM programming, mailbox/CCI,
  events, poison, regions, memory tasks, PM
- Validation interfaces: DTS, testcase channel, ioctl ABI, `MODEL_TRACE`,
  coverage report, performance windows, console utilities
- Configurable ports `N = 1..32`; NTB/DM minimal model-defined scope (F1a/F1b)
- The only RTL in this repository is the port adapter/tie-off/DPI stub.
  PHY/link training/DLL retry belong to the downstream CTL RTL.

## Layout

```
include/                 public C++ headers
lib/libcxl_switch_model.a binary model library (no HDL simulator required)
console/                 standalone console (tc/perf/trace/lspci/cxl list)
smoke/                   independent-compile smoke
dts/onetest-cxl.dts      runtime profile template
rtl_port/                port adapter, tie-off, DPI stub and `.veo`
tb/uvm_ioctl/            UVM TB + ioctl-style test selector (porting entry point)
cosim/cxl/axi/           optional 128-bit AXI4 master + external-SRAM bench
tools/                   port check, DUT vector/force generators
USER_GUIDE_CXL.md        build/API/console/TC/ioctl/trace/DTS guide
NOT_SUPPORTED_CXL.md     scope, IP ledger, not-supported list
evidence/VALIDATION.md   validation summary
```

## Quick start

```sh
make -C smoke run
make -C console
./console/cxl_switch_console --run-all
./console/cxl_switch_console --tc all
./console/cxl_switch_console --tc-cover
```

The library is self-contained and has no SystemC or OS dependency.  A customer
integration can link `lib/libcxl_switch_model.a`, include `include/cxl_*.h`,
and drive the model through the C++ classes or the testcase/ioctl channels.

Note (2026-11): the model upper half (root, switch USP/crossbar, N×swdn
ports) also has an internal SystemC implementation used for VCS/DPI
co-simulation.  Both layers share the same U-layer objects and behavior;
the SystemC layer (`cxl_sc_*`) is deliberately not part of this
SystemC-free delivery archive.

### Start here for porting: UVM TB with ioctl-style test control

`tb/uvm_ioctl/` is a small UVM testbench whose `run_phase` polls a Unix-domain
socket and dispatches testcases.  It is the recommended starting point for
integrating the model into a customer UVM environment; the UVM/socket/CLI
scaffolding is independent of the test bodies and can be kept while the
dispatcher is replaced with customer sequences.

```sh
sh tb/uvm_ioctl/run.sh build
sh tb/uvm_ioctl/run.sh sim
sh tb/uvm_ioctl/run.sh list
sh tb/uvm_ioctl/run.sh run all 1
sh tb/uvm_ioctl/run.sh coverage
sh tb/uvm_ioctl/run.sh quit
```

Set `VCS` to the customer simulator command if it is not `vcs`; see
`tb/uvm_ioctl/README.md` for porting notes.

AXI4 host-DDR attachment (optional; external SRAM acts as motherboard DDR):

```sh
sh cosim/cxl/axi/run.sh          # xsim; SIM=vcs selects VCS
```

DTS profile:

```sh
./console/cxl_switch_console --dts dts/onetest-cxl.dts --run-all
```

## Validation status

The model-side feature set is verified by a **115-gate full-tree regression
(0 FAIL)** that includes a 43-bench capability suite, plus DTS,
testcase/ioctl/trace/coverage/perf, DUT-docking vectors, port compatibility
check, independent library smoke, and console runs.  A local integration of
four `swdn` RTL cores + four RTL EPs passes MC/BC/MemRd M1-M5 (14/14).  See
`evidence/VALIDATION.md` for counts and artifacts.  Customer `swdn` + EP DUT
RTL co-simulation and the RTL PBR/256B profile remain pending the customer
environment and are reported as `SKIP`, never as a pass; model-side known
limits are enumerated in `NOT_SUPPORTED_CXL.md`.

## Boundaries

See `NOT_SUPPORTED_CXL.md`.  In short: no NTB/DM; no full PCIe extended
capability surface inside vPPB; no vendor-specific slot power timing or GIM
internal GFD mapping; no CXL.cache TEE; no PHY-level behavior; no physical
media poison path; no claim of kernel timing.

**No downstream-port (`swdn`) or endpoint RTL is included.**  The only RTL in
this repository is the port adapter/tie-off/DPI stub; the UVM TB is a
control/porting example, not a DUT model.

This is a functional reference model; it does not replace RTL/silicon
verification.
