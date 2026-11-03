# UVM testbench with ioctl-style test control

This is the recommended **starting point for porting** the CXL model into a
customer verification environment.  It is a small UVM testbench whose
`run_phase` polls a Unix-domain socket and dispatches testcases through a
DPI-C shim to the model library.

The control surface is intentionally independent of the model test bodies:
keep the UVM/socket/CLI scaffolding and replace the test dispatcher with your
own sequences, scoreboards, or RTL/VIP tests.

## Layout (installed at `<repo>/tb/uvm_ioctl/`)

```
cxl_uvm_tb.sv          UVM TB: run_phase polls the control socket
cxl_uvm_ctl.h/.cpp     DPI shim + Unix-socket control transport
cxl_uvm_ctl_cli.cpp    user CLI: list/run/coverage/quit
run.sh                 build/run helper
README.md              this file
```

The model library and public headers are in `<repo>/lib` and `<repo>/include`.
`run.sh` derives the repository root as `../..` from this directory.

## Prerequisites

- A SystemVerilog simulator with UVM-1.2.
- A C++17 compiler (`g++` by default).
- Set `VCS` to your simulator command if it is not `vcs`.
- For VCS, `SIM_ARGS="-no_save"` is convenient.

```csh
setenv VCS /path/to/vcs
setenv CXX g++
```

## Build and run

```sh
sh run.sh build
sh run.sh sim
sh run.sh list
sh run.sh run 0
sh run.sh run all 1
sh run.sh coverage
sh run.sh quit
```

Command format (one command per socket connection):

```text
LIST
RUN <id|all> [iterations]
COVERAGE
QUIT
```

The socket path is `/tmp/cxl_uvm_ctl.sock`.  Any Unix-socket client can send
these commands; the supplied CLI is just a convenience.

Example result with the delivered model library:

```text
RUN all 1: ok=1 ran=18 passed=18 failed=0 rc=0
COVERAGE total=18 tested=18 untested=0
QUIT: BYE
```

This example is a control/orchestration check.  It is not an RTL or VIP test
pass; customer DUT/VIP integration is separate.

## Porting notes

1. **Keep the socket/CLI layer.**  `cxl_uvm_ctl.h` defines the transport and
   the four command codes.  It has no simulator-specific dependency.
2. **Replace the test body dispatch.**  In `cxl_uvm_ctl.cpp`,
   `cxl_uvm_ctl_run()` calls `cxl_tc_execute()` in the model library.  For a
   customer environment, replace that call with your own dispatcher (or add
   DPI functions) that starts your UVM sequences / drives your DUT.
3. **UVM side.**  `cxl_uvm_tb.sv` only polls and prints; no model internals are
   referenced.  You can instantiate your own UVM environment and call the four
   `cxl_uvm_ctl_*` completion functions after each command.
4. **Test selection.**  The delivered model library includes the testcase
   registry in `include/cxl_tc.h` (`CXL_TC_GET_COUNT/GET_INFO/RUN/COVERAGE`).
   Customers can keep it as a reference or map the same IDs to their own tests.
5. **RTL/VIP integration.**  Add your RTL/VIP compile and topology settings to
   `run.sh` (or your own script); the UVM TB itself has no PHY/VIP dependency.

## Boundary

The UVM TB is a control shell and porting template.  It does not claim to be a
complete DUT/VIP regression, and its model testcase results do not replace the
customer RTL/VIP sign-off.
