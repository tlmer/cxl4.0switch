# Using a Customer CXL VIP as an Endpoint — CXL.io Enumeration and Memory Test Guide

This guide explains how to reproduce the **single-port CXL.io link-up,
configuration-enumeration, memory-read, and posted memory-write/readback**
test with this model acting as the host and a customer CXL VIP instance
acting as the endpoint.  The model-side reference result is: link up at
8 Gb/s x1, 5 devices enumerated / 0 skips, MemRd PASS, and posted
MemWr/readback PASS (`0xa5c3e1f0`).  The internal evidence package is not
part of this repository.

## Scope

**Validated by this flow**

- CXL.io link training between the model host (via the SW/CTL downstream
  port DUT) and a VIP endpoint.
- CXL.io configuration reads and writes: BAR sizing/programming,
  `Command.MSE`, PCIe/AER/PM capability updates, DVSEC/Register-Locator
  enumeration.
- **CXL.io memory read** from the model host to the VIP endpoint BAR (4-byte
  MemRd to BAR0, completion status 0), with the model's SV host-agent memory
  transport enabled.
- **CXL.io posted memory write and readback**: 4-byte MemWr to BAR0+4
  (`0x80000004`), completion of the posted write, and a readback returning
  the written pattern (reference: `0xa5c3e1f0`).
- Model-side enumeration verdict: 5 devices, 0 skips, with Linux-style PCI
  log and `lspci -vvvx` output.

**Not validated by this flow**

- CXL.cache / CXL.mem over the serdes link.  The current CTL configuration is
  Gen3 and CXL APN requires >=32 GT/s; the VIP reports `APN_DISABLED` and the
  cache/mem vLSM never activates.  Those protocols are validated in the
  separate VIP TL route-C flow.
- Final VIP/UVM test-suite verdict: the reference run was stopped after the
  model-side memory smoke.

## Roles and topology

```
model host (C++ enumeration/config policy)
   |  DPI: cxl_host_agent / cxl_dbi_agent
SW DUT: sw_vip_dut_model (CTL CXL controller + PHY model)
   |  serial PCIe/CXL link (Gen3 x1 in the reference run)
VIP endpoint: svt_cxl_subsystem VIP, Type-3/CXL.io device
```

The VIP testbench's own host agent is replaced by the model host.  The VIP
test's CXL.io random-memory sequence expects the VIP host-side enumeration
and address ranges; that sequence is therefore not used as a pass criterion
in this flow.  Configuration traffic from the model host is the CXL.io test.

## Prerequisites

### 1. VIP installation and a writable work tree

- CXL Subsystem VIP (SVT) **X-2025.09** (the overlay is written for this
  version).
- Copy the example testbench to a writable work tree (do not edit the VIP
  installation in place):

  ```sh
  cp -a <VIP_HOME>/examples/sverilog/cxl_test_suite_svt/tb_dut_cxl \
        <WORK>/tb_dut_cxl
  ```

- Point the model's VIP path at that work tree.  In this tree
  `cosim/cxl/vip_design` is a symlink to the VIP work tree; replace it:

  ```sh
  cd <MODEL_ROOT>/cosim/cxl
  rm -f vip_design
  ln -s <WORK> vip_design
  ```

  Alternatively set `DESIGNWARE_HOME` to the VIP installation for the build
  and run (the wrapper uses `DESIGNWARE_HOME` when set).

### 2. Overlay files (testbench files added/modified for this model)

The overlay is delivered separately under the customer's VIP license; it is
not part of this repository.  Unpack it under `<OVERLAY>` (the overlay root
mirrors the `tb_dut_cxl` relative paths below).  Copy each file into the same
relative path under `<WORK>/tb_dut_cxl`.  Do not redistribute the overlay
with the model.

| Relative path | Purpose |
|---|---|
| `top.sv` | Includes only the CXL.io test used here; adds wave control and CXL/PCIe package imports. |
| `tests/ts.cxl_io_random_mem_wr_rd.sv` | Test: explicit APN-capable bits, Gen3/width selection, config-database identity, full configuration-space dump. |
| `topology_snps_sw_vip_model.svi` | Topology: SW DUT + VIP endpoint; model host DPI attached to the SW DUT app face. |
| `compile_snps_sw_vip_model.f` | File list for VIP + CTL SW model + model-side wrappers. |
| `sw_vip_dut_model.sv` | CTL SW/model-host wrapper (`cxl_host_agent`, `cxl_dbi_agent`). |
| `sw_vip_dut_serial.sv`, `sw_vip_dut.sv`, `sw_vip_dut_instance.svi` | Earlier DUT wrappers, kept for reference. |
| `top_files`, `hdl_files` | Top-file list (see path edits below). |
| `sim_run_options` | Default run plusargs (`+cxl_vip_width=4`, `+sw_vip_link_delay_us=60`, verbosity). |
| `vcs_build_options` | VCS compile options (timescale, libext, incdirs, defines). |
| `vcs_elab_options` | VCS link options (`-P pli.tab msglog.o -L<MODEL_ROOT>/cosim/cxl/<pair-dir> -lcxl_host_dpi`). |
| `links.txt` | Documents the two required symlinks. |

Recommended copy commands:

```sh
TB=<WORK>/tb_dut_cxl
OVERLAY=<OVERLAY>
cp -p $OVERLAY/top.sv $TB/
cp -p $OVERLAY/tests/ts.cxl_io_random_mem_wr_rd.sv $TB/tests/
cp -p $OVERLAY/topology_snps_sw_vip_model.svi $OVERLAY/compile_snps_sw_vip_model.f $TB/
cp -p $OVERLAY/sw_vip_dut_model.sv $OVERLAY/sw_vip_dut_serial.sv \
      $OVERLAY/sw_vip_dut.sv $OVERLAY/sw_vip_dut_instance.svi $TB/
cp -p $OVERLAY/top_files $OVERLAY/hdl_files $OVERLAY/sim_run_options $TB/
cp -p $OVERLAY/vcs_build_options $OVERLAY/vcs_elab_options $TB/
cd $TB
ln -sfn compile_snps_sw_vip_model.f    svt_cxl_subsystem_compile_file.f
ln -sfn topology_snps_sw_vip_model.svi svt_cxl_subsystem_topology_file.svi
```

`run_dut_cxl`'s `prescript` recreates those two symlinks on **every build and
run**.  It selects the model topology only when
`SVT_CXL_SUBSYSTEM_TOPOLOGY_FILE` and `SVT_CXL_SUBSYSTEM_COMPILE_FILE` are
exported; otherwise it silently falls back to the stock VIP-to-VIP pair and
the model host will not appear.  Export them for the build and run (see the
Build section).  A correct build reports `1292 modules and 20 UDPs read`;
the stock fallback reports `80 modules and 0 UDP read`.

These overlay files are testbench integration files.  No VIP source file is
modified or redistributed; the VIP remains under the customer's license.

### 3. Model-side files and the DPI library

The compile file references these model files (under `<MODEL_ROOT>`):

- `cosim/cxl/<pair-dir>/cxl_app_adapter.sv`, `cxl_sw_phy.sv`,
  `sw_vip_dut_core.sv`
- `cosim/cxl/<pair-dir>/cxl_host_agent.sv`, `cxl_dbi_agent.sv`
- `cosim/cxl/<pair-dir>/rams_local/*`, `dut_ep/*`, `stim/*`

`<pair-dir>` is the controller-pair directory in the delivered model tree.

Build the model host DPI library:

```sh
cd <MODEL_ROOT>
sh cosim/cxl/<pair-dir>/build_host_dpi.sh
```

This creates `cosim/cxl/<pair-dir>/libcxl_host_dpi.so`.  The testbench
elaboration must link it; in our tree this is recorded in the TB's
`vcs_elab_options`:

```text
-L<MODEL_ROOT>/cosim/cxl/<pair-dir> -lcxl_host_dpi
```

At run time the loader must find the `.so` (place a copy in `$TB` or put
`<MODEL_ROOT>/cosim/cxl/<pair-dir>` on `LD_LIBRARY_PATH`).  Run `run_dut_cxl`
with `-common_lib_dyn_load`; otherwise it unsets `LD_LIBRARY_PATH` and the
library is not found.

### 4. Absolute paths to replace

Our reference compile file embeds local paths.  Replace them with the
customer's paths:

| In file | Replace | With |
|---|---|---|
| `compile_snps_sw_vip_model.f` | the controller config path (`config_sw3cxl/...`) | `$CTL_SW_HOME/config_sw3cxl/...` |
| `compile_snps_sw_vip_model.f` | the endpoint config path (`config_epx1cxl/...`) | `$CTL_SW_HOME/config_epx1cxl/...` |
| `compile_snps_sw_vip_model.f` | `<MODEL_ROOT>/...` | `$MODEL_ROOT/...` |
| `compile_snps_sw_vip_model.f` | `<FPGA_HOME>/data/...` | `$FPGA_HOME/data/...` (or remove unused primitives) |
| `top_files` | absolute path to our `top.sv` | `$TB/top.sv` (or a relative `top.sv`) |
| `vcs_elab_options` | `-L<MODEL_ROOT>/cosim/cxl/<pair-dir>` | `-L$MODEL_ROOT/cosim/cxl/<pair-dir>` |

The `config_sw3cxl` / `config_epx1cxl` trees are the customer's CTL
configuration sources (the same trees used to build the SW RTL).
The FPGA-vendor entries provide `glbl` and the unisim primitives used by the
SW model; point them at the customer's FPGA-vendor tool installation.

### 5. Tools and environment

- VCS X-2025.06 (or a compatible version), `csh`, g++.
- A valid VIP license for the run.
- `DESIGNWARE_HOME` pointing at the VIP installation or work tree.
- `VCS_HOME`, `PATH`, `LD_LIBRARY_PATH` set as usual for VCS.
- For the CXL.io memory read, set `CXL_HOST_MEM_TRANSPORT=1` and
  `CXL_HOST_MEM_SMOKE=1`; `CXL_HOST_MEM_TIMEOUT_S` bounds the transaction.
  Add `CXL_HOST_MEM_SMOKE_WRITE=1` for the posted write/readback test.
- The model tree and the CTL SW configuration tree readable by the build.

## Build

Export the model topology/compile pair **before building**.  The testbench's
`prescript` recreates both `svt_cxl_subsystem_*` symlinks at every build and
run; if these variables are unset it silently falls back to the stock
VIP-to-VIP topology, the model host never appears, and no `MODEL_HOST_*`
prints are produced.

```sh
cd $TB
export SVT_CXL_SUBSYSTEM_TOPOLOGY_FILE=topology_snps_sw_vip_model.svi
export SVT_CXL_SUBSYSTEM_COMPILE_FILE=compile_snps_sw_vip_model.f
./run_dut_cxl -buildonly cxl_io_random_mem_wr_rd vcsvlog
```

Expected products: `output/simvcssvlog` and its `.daidir`.  The build is the
expensive step; the reference build uses 1292 modules / 20 UDPs.

Before running, verify that the build really used the model topology:

```sh
readlink -f svt_cxl_subsystem_topology_file.svi
# must end in .../topology_snps_sw_vip_model.svi
readlink -f svt_cxl_subsystem_compile_file.f
# must end in .../compile_snps_sw_vip_model.f
```

## Run

From the model tree:

```sh
cd <MODEL_ROOT>
CXL_HOST_MEM_TRANSPORT=1 CXL_HOST_MEM_SMOKE=1 \
CXL_HOST_MEM_SMOKE_WRITE=1 \
CXL_HOST_MEM_TIMEOUT_S=60 CXL_HOST_CFG_TIMEOUT_S=120 \
sh cosim/cxl/w5_host/run_host_enum.sh cxl_io_random_mem_wr_rd
```

The wrapper:

- sets `SVT_CXL_SUBSYSTEM_TOPOLOGY_FILE=topology_snps_sw_vip_model.svi`,
  `SVT_CXL_SUBSYSTEM_COMPILE_FILE=compile_snps_sw_vip_model.f`,
  `DESIGNWARE_HOME`, and `LD_LIBRARY_PATH`;
- runs the compiled testbench with `-nobuild -common_lib_dyn_load`;
- writes the full log to `/dev/shm/w5_host_run.log`, the model host trace
  to `/dev/shm/w5_model_host_trace.txt`, and the Linux-side evidence
  (enumeration lines + `lspci -vvvx`) to `/dev/shm/w5_linux_side_prints.txt`;
- checks `MODEL_HOST_ENUM: PASS (5 devices, 0 skips)`, the five role lines,
  and the `02:00.0` DSP / `03:00.0` VIP EP addresses.

For a manual run, use the same environment and:

```sh
cd $TB
csh -f ./run_dut_cxl -nobuild -common_lib_dyn_load \
    cxl_io_random_mem_wr_rd vcsvlog
```

Speed/width selection: the test's `create_cfg` reads
`+cxl_vip_speed=<1..4>` (2.5G/5G/8G/32G; default 8G) and
`+cxl_vip_width=<1|2|4>`.  The current `run_dut_cxl` in this example does not
forward a free-form `RUN_ARGS`; put plusargs in the TB's `sim_run_options`
file if you need a non-default speed.  The `+svt_pcie_link_speed=GEN5`
plusarg of the VIP test suite is overridden by `create_cfg` in this example.

## Expected prints

Model-side evidence (`/dev/shm/w5_linux_side_prints.txt`, copied to the
evidence package as `model_side_prints.txt`) contains **only** Linux-style
enumeration lines and `lspci -vvvx`; model-internal debug (`MODEL_HOST_*`,
DBI traces) is kept out of that file.

```text
[    0.000000] pci 0000:00:00.0: [1e98:0003] type 00 class 0x060000
...
[    0.007000] pci 0000:03:00.0: [abcd:ef90] type 00 class 0x050210
[    0.008000] cxl_pci 0000:03:00.0: CXL Type3 device probed
lspci -vvvx
00:00.0 Host bridge [1e98:0003]
...
02:00.0 PCI bridge [1234:5678]
	Control: I/O- Mem+ BusMaster+ ...
	...
03:00.0 CXL memory device [abcd:ef90]
	Control: I/O- Mem+ BusMaster+ ...
	00: cd ab 90 ef 06 00 00 00 ...
```

Model log lines (used by the wrapper):

```text
MODEL_HOST: DUT link ready; starting enumeration
MODEL_HOST_MEM: bus=3 dev=0 fn=0 bar=0 rd addr=0x80000000 PASS data=00000000
[CXL_HOST_AGENT] posted mem write accepted addr=0x80000004 data=a5c3e1f0 tag=08
MODEL_HOST_MEM: wr addr=0x80000004 pat=a5c3e1f0 rdback PASS data=a5c3e1f0 expected=a5c3e1f0
MODEL_HOST_ENUM: PASS (5 devices, 0 skips)
MODEL_HOST_ENUM_WHERE: model USP + DUT DSP(DBI) + VIP EP(XALI1/RCPL)
```

VIP side:

```text
LTSSM: Link training completed. The link is READY! Speed is 8Gb/s.
Link width is 1. ...
CplD ... Cpl ID=... Cpl Status=0x0, BCM=0, Byte Count=4
DisplayStatsLight: PCIE Target Stats:
    Total received TLPs = 292, Total received REQs = 292
    Total received bytes = 3608, Total received data bytes = 104
```

In our Gen3 reference run the VIP also reports two equalization
`UVM_ERROR`s on the SW DUT/PHY model
(`phy_rx_packet_before_equalization_complete`,
`phy_equalization_bypassed`) and one link-number warning.  They come from the
SW PHY model bypassing equalization at Gen3; they are not CXL.io transaction
failures, and all config completions have status 0.  If the customer's SW
PHY model implements equalization, these checks should be clean.

## Pass criteria

- Model trace contains `MODEL_HOST_ENUM: PASS (5 devices, 0 skips)`.
- Model log contains `MODEL_HOST_MEM: ... rd addr=0x80000000 PASS` when
  `CXL_HOST_MEM_TRANSPORT=1` and `CXL_HOST_MEM_SMOKE=1` are set.
- With `CXL_HOST_MEM_SMOKE_WRITE=1`, the log contains
  `[CXL_HOST_AGENT] posted mem write accepted ...` and
  `MODEL_HOST_MEM: wr ... rdback PASS data=<pat> expected=<pat>` with
  `data == expected`.
- The five role lines and the DSP/VIP EP addresses match the trace above.
- VIP link training completes, config CplD status is 0, and the model
  `MODEL_HOST_CFG_DONE` writes report `status=0`.
- Treat VIP equalization compliance errors separately from CXL.io transaction
  errors; do not count a CXL.io case as passing if its own CplD status is
  non-zero.
- The reference run was stopped after the model-side memory smoke; it does
  not claim a final VIP/UVM verdict.  The model-side enumeration/config/
  memory criteria above are the pass criteria for this flow.

## Known limitations / troubleshooting

- **No `MODEL_HOST_ENUM` / no `sw_vip_dut_model`**: the TB was built with the
  stock VIP-to-VIP topology/compile pair.  `prescript` recreates both
  symlinks on every build and run and silently falls back to
  `topology_snps_vip_cxl_b2b.svi` / `compile_snps_vip_pcie_serial.f` when
  `SVT_CXL_SUBSYSTEM_TOPOLOGY_FILE` / `SVT_CXL_SUBSYSTEM_COMPILE_FILE` are
  unset.  Export both variables for the **build** and the run.  The correct
  build reports `1292 modules and 20 UDPs read`; the stock fallback reports
  `80 modules and 0 UDP read`.
- **`libcxl_host_dpi.so` not found**: run with `-common_lib_dyn_load` and put
  the `.so` on `LD_LIBRARY_PATH` (or in `$TB`).
- **Missing `glbl`/unisim**: update the file list's FPGA-vendor primitives
  path (`<FPGA_HOME>/data/...`) to the customer's installation.
- **`APN_DISABLED` / no cache/mem vLSM**: expected on a Gen3 link.  CXL APN
  requires >=32 GT/s; use a Gen5 CTL/PHY configuration, or use the VIP TL
  route-C flow for CXL.cache/mem.
- **Model MemRd to the VIP BAR fails**: set `CXL_HOST_MEM_TRANSPORT=1` (and
  `CXL_HOST_MEM_SMOKE=1`).  Without the transport switch, `mem_enabled()` is
  false and the model returns immediately (`mem_transport=0`); this is not a
  VIP memory-window issue.
- **Posted write times out**: the posted-write FSM must retire the TLP on
  the XADM acceptance cycle (halt low with hv/eot).  Waiting for two
  consecutive halt-low cycles deadlocks because arbitration can reassert
  halt immediately after accepting.  The memory tag must also be a legal
  5-bit tag unless Extended Tag Enable is set.
- **VIP test-suite random-memory sequence**: it expects the VIP host-side
  enumeration/ranges.  With the model host replacing the VIP host, use a
  dedicated sequence that drives the desired CXL.io memory traffic.

## Security / licensing

The overlay contains only testbench integration files.  Do not redistribute
the VIP vendor's source or generated VIP files; the customer's VIP license and
installation remain in effect.
