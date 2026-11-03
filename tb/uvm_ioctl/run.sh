#!/bin/sh
# -----------------------------------------------------------------------------
# run.sh -- build/run the CXL UVM TB with the ioctl-style test selector.
#
#   sh run.sh build
#   sh run.sh sim                 # start the UVM TB in the background
#   sh run.sh list                # list the registered model testcases
#   sh run.sh run 0               # run testcase 0
#   sh run.sh run all 1           # run all testcases once
#   sh run.sh coverage            # tested / untested + reasons
#   sh run.sh quit                # stop the UVM TB
#   sh run.sh stop                # kill it / remove the socket
#
#   Environment:
#     VCS       SystemVerilog simulator command (default: vcs)
#     CXX       C++ compiler (default: g++)
#     SIM_ARGS  extra simulator arguments (e.g. -no_save for VCS)
#
#   This directory is installed at <repo>/tb/uvm_ioctl; the model library and
#   public headers are at <repo>/lib and <repo>/include.
# -----------------------------------------------------------------------------
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
VCS=${VCS:-vcs}
CXX=${CXX:-g++}
SIM="$HERE/simv"
CTL="$HERE/cxl_uvm_ctl"
SOCK=/tmp/cxl_uvm_ctl.sock

build_lib() {
    echo "[ctl] building libcxl_uvm_ctl.so"
    $CXX -std=c++17 -O2 -fPIC -shared \
        -I"$ROOT/include" -I"$HERE" \
        "$HERE/cxl_uvm_ctl.cpp" \
        "$ROOT/lib/libcxl_switch_model.a" \
        -lpthread -ldl -lm \
        -o "$HERE/libcxl_uvm_ctl.so"
    echo "[ctl] building cxl_uvm_ctl CLI"
    $CXX -std=c++17 -O2 -I"$HERE" "$HERE/cxl_uvm_ctl_cli.cpp" \
        -o "$CTL"
}

build_sim() {
    echo "[uvm] simulator build"
    rm -f "$SIM"
    "$VCS" -full64 -sverilog -ntb_opts uvm-1.2 -timescale=1ns/1ps \
        -top cxl_uvm_tb_top \
        -CFLAGS "-I$ROOT/include -I$HERE" \
        -L"$HERE" -lcxl_uvm_ctl -lstdc++ -lpthread -ldl -lm \
        -o "$SIM" \
        "$HERE/cxl_uvm_tb.sv"
}

case "${1:-}" in
    build) build_lib; build_sim ;;
    sim)
        [ -x "$SIM" ] || { echo "missing $SIM; run: sh $0 build" >&2; exit 1; }
        # Run from the repository root so dts/onetest-cxl.dts resolves.
        ( cd "$ROOT" && LD_LIBRARY_PATH="$HERE:${LD_LIBRARY_PATH:-}" \
            "$SIM" +UVM_TESTNAME=cxl_uvm_ioctl_test ${SIM_ARGS:-} \
            > "$HERE/sim.log" 2>&1 & echo $! > "$HERE/sim.pid" )
        echo "started UVM TB pid=$(cat "$HERE/sim.pid"); log=$HERE/sim.log"
        ;;
    list|run|coverage|quit)
        [ -x "$CTL" ] || { echo "missing $CTL; run: sh $0 build" >&2; exit 1; }
        LD_LIBRARY_PATH="$HERE:${LD_LIBRARY_PATH:-}" exec "$CTL" "$@"
        ;;
    stop)
        if [ -f "$HERE/sim.pid" ]; then
            kill "$(cat "$HERE/sim.pid")" 2>/dev/null || true
            rm -f "$HERE/sim.pid"
        fi
        rm -f "$SOCK"
        ;;
    *)
        echo "usage: sh $0 {build|sim|list|run <id|all> [n]|coverage|quit|stop}" >&2
        exit 2
        ;;
esac
