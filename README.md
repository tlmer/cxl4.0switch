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
- Type-1 prefetchable memory windows (0x24/0x28/0x2C, 32/64-bit forms) and
  CXL.io P2P routing through them; local and remote enumeration assign DSP/USP
  bridge windows from the actual endpoint BARs, including the parent DSP of a
  cascaded switch; P2P into a cascade resolves the child port and backend
- CXL.cache Type-2 MESI, multi-device directory, CQID out-of-order, LRU limits, owner replacement hooks, and a TEE trust boundary (secure ranges, line tags, owner protect/integrity hooks)
- PBR: PTH, SPID/DPID, FAST/LDST/IDT, Direct P2P, multi-level fabric/ISL/G-FAM
- PBR switch CCI, RGT 256 / Table 7-87 mix and advanced congestion avoidance
- MLD/FMLD/vCS, vPPB Type-1 configuration, hot-plug/power sequencing
- EMD/BISnp/BIRsp/HDM-DB, multi-host dirty lines
- UIO Direct P2P, GMV/VTV/GIM, FAST/GIM interleave
- Linux proxy: ACPI/CEDT view, discovery, HDM programming, mailbox/CCI,
  events, poison, regions, memory tasks, PM
- W5 model-host co-simulation: 5-device tree (host bridge/root port/USP/DSP/
  endpoint), CXL.io read completions with matching data, DUT traffic gated
  until model enumeration completes
- Mailbox: CXL 4.0 Table 8-215 command set, plus an owner hook for unlisted
  opcodes (GFD 0x4900-0x4914, PBR switch CCI 0x5717+, vendor commands) with
  explicit Unsupported when no handler is installed
- Validation interfaces: DTS, testcase channel, ioctl ABI, `MODEL_TRACE`,
  coverage report, performance windows, console utilities
- Security Send/Receive envelope with a reference TCG/SED handler (TCG framing, Level 0 Discovery, lock/passphrase/revert state); IDE AES-256-GCM and DOE/SPDM models
- DCD capacity/extent model with a reference sparse media backend (read/write/sanitize/scan), wired to Type3 Scan Media and Media Operation
- GAM Buffer, GFD enhanced-event payloads and a GAE endpoint composition
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

The delivered library is SystemC-free.  Optional SystemC/DPI co-simulation
integration is available separately; it is not part of this archive.

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

### Calling the model from a customer UVM environment

Do not restructure your UVM testbench around the model.  The model is adapted
to the customer TB through a bridge/agent package supplied as part of the
integration.  The customer keeps its existing environment, sequences,
scoreboard and tests; only a bridge instance and its bindings are added.

**Mode A -- pin-level bridge (customer RTL DUT)**

1. File to instantiate:

   `cxl_model_bridge.sv`

   Supplied in the integration package.  It exposes the grouped CXL face
   defined by `rtl_port/cxl_top_port_grouped.sv`.  Connect the customer DUT's
   CXL pins to it.  `cxl_top_port_grouped.sv` is only the port contract, not
   the model.

2. VCS command to load the model:

   ```sh
   vcs -full64 -sverilog -timescale=1ns/1ps \
       -sv_lib <BRIDGE>/build/libcxl_model_bridge \
       <your_tb_files> <BRIDGE>/cxl_model_bridge.sv
   ```

   The `.so` contains `libcxl_switch_model.a` and the DPI bridge.  No socket,
   CLI or model test shell is needed.

Keep the existing UVM sequences and scoreboard; only add the bridge instance
and the pin connections.

**Mode B -- TLM/UVM bridge (customer VIP-based TB)**

The bridge is a standard UVM agent with sequencer/driver/analysis ports.

1. Connect the agent where the existing host/switch model or host VIP was.
2. Your existing sequences and scoreboard remain unchanged.
3. The agent converts your transaction type through an adapter layer that is
   part of the supplied bridge; the customer does not implement the DPI
   protocol or rewrite the TB.
4. The agent calls the same C++ model through DPI.

**Common integration steps**

1. Provide the TB interface: RTL pins or UVM transaction type, number of
   downstream ports, flit mode, DUT address map, and optional host-memory/
   AXI backend.  The matching bridge is generated/configured for that
   interface.
2. For pin mode, load the delivered DPI bridge with VCS `-sv_lib`; for TLM
   mode, link the UVM agent/DPI library.  No socket, CLI, or specific UVM
   test is required.
3. Add the bridge instance to the existing env/top and bind the existing
   pins or TLM ports.
4. Set model configuration: port count, 68B/256B/PBR profile, HDM/AXI
   backend and timeouts.
5. Run the existing tests.  Model completions, snoops, memory data and
   events are returned through the bridge.

The model is a host/root/switch component; the customer DUT/VIP remains the
downstream endpoint.  The integration work is delivered on the model side as
the bridge/agent; the customer TB does not need to adopt the model's own test
shell or internal structure.

AXI4 host-DDR attachment (optional; external SRAM acts as motherboard DDR):

```sh
sh cosim/cxl/axi/run.sh          # xsim; SIM=vcs selects VCS
```

DTS profile:

```sh
./console/cxl_switch_console --dts dts/onetest-cxl.dts --run-all
```

## Validation status

The model-side feature set was verified by a **115-gate model-side
regression (0 FAIL)** that includes a 43-bench capability suite, plus DTS,
testcase/ioctl/trace/coverage/perf, DUT-docking vectors, port compatibility
check, independent library smoke, and console runs.  The current tree has
129 gates; the added R7-R21 feature gates were run directly and pass, while
a complete 129-gate rerun has not been performed, so 129/129 is not claimed.  A local integration of
four `swdn` RTL cores + four RTL EPs passes MC/BC/MemRd M1-M5 (14/14).  See
`evidence/VALIDATION.md` for counts and artifacts.  Customer `swdn` + EP DUT
RTL co-simulation and the RTL PBR/256B profile remain pending the customer
environment and are reported as `SKIP`, never as a pass; model-side known
limits are enumerated in `NOT_SUPPORTED_CXL.md`.

## Boundaries

See `NOT_SUPPORTED_CXL.md`.  In short: NTB/DM are minimal model-defined
scope; the vPPB has a broad but not complete PCIe extended-capability
surface; no vendor-specific slot power timing or GIM internal GFD mapping;
CXL.cache TEE is a functional trust boundary with owner crypto/key policy;
no PHY-level behavior; no physical media poison path; no
claim of kernel timing.

**No downstream-port (`swdn`) or endpoint RTL is included.**  The only RTL in
this repository is the port adapter/tie-off/DPI stub; the UVM TB is a
control/porting example, not a DUT model.

This is a functional reference model; it does not replace RTL/silicon
verification.
