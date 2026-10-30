# CXL switch Model

Repository: <https://github.com/tlmer/cxl4.0switch>

A functional C++17 model of a **CXL 4.0 root complex, switch upper half
(USP/crossbar/downstream-port adapters), and a no-OS Linux behavior proxy**.
The customer connects one or more CTL CXL switch downstream-port (`swdn`)
instances and endpoint DUTs behind them; this repository provides the host-side
model, the port-adapter RTL, a binary library, public headers, a console, a DTS
template, and validation artifacts.

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
tools/                   port check, DUT vector/force/wrapper generators
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

DTS profile:

```sh
./console/cxl_switch_console --dts dts/onetest-cxl.dts --run-all
```

## Validation status

The model-side feature set is verified by 44 feature testbenches (600 checks)
plus DTS, testcase/ioctl/trace/coverage/perf, DUT-docking vectors, port
compatibility check, independent library smoke, and console runs.  See
`evidence/VALIDATION.md` for counts and artifacts.  Customer `swdn` + EP DUT
RTL co-simulation and the RTL PBR/256B profile remain pending the customer
environment and are reported as `SKIP`, never as a pass.

## Boundaries

See `NOT_SUPPORTED_CXL.md`.  In short: no NTB/DM; no full PCIe extended
capability surface inside vPPB; no vendor-specific slot power timing or GIM
internal GFD mapping; no CXL.cache TEE; no PHY-level behavior; no physical
media poison path; no claim of kernel timing.

This is a functional reference model; it does not replace RTL/silicon
verification.
