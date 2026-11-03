#!/bin/sh
# Build and run the released model's 128-bit AXI4 host-DDR self-test.
#   SIM=xsim (default) | SIM=vcs
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
SIM=${SIM:-xsim}
cd "$ROOT"
mkdir -p build

CXX=${CXX:-g++}
$CXX -std=c++17 -O2 -fPIC -shared -pthread -I include \
    "$HERE/cxl_axi_dpi_link.cpp" \
    -L lib -lcxl_switch_model -o build/libcxl_axi_dpi.so

if [ "$SIM" = "vcs" ]; then
    VCS=${VCS:-vcs}
    cd "$HERE"
    $VCS -full64 -sverilog -timescale=1ns/1ps -top tb_cxl_axi_sram \
        -sv_lib "$ROOT/build/libcxl_axi_dpi" \
        cxl_axi_dpi_master.sv axi_ram_ref.v tb_cxl_axi_sram.sv \
        -o "$ROOT/build/simv_cxl_axi_sram" \
        -l "$ROOT/build/cxl_axi_sram_compile.log"
    set +e
    "$ROOT/build/simv_cxl_axi_sram" -l "$ROOT/build/cxl_axi_sram_run.log" \
        > "$ROOT/build/cxl_axi_sram_stdout.log" 2>&1
    RC=$?
    set -e
else
    W="$ROOT/build/axi_xsim"
    rm -rf "$W"; mkdir -p "$W"; cd "$W"
    xvlog -sv "$HERE/cxl_axi_dpi_master.sv" "$HERE/axi_ram_ref.v" \
        "$HERE/tb_cxl_axi_sram.sv" > "$ROOT/build/cxl_axi_sram_xvlog.log" 2>&1
    xelab -sv_lib ../libcxl_axi_dpi tb_cxl_axi_sram -s axi_sim \
        > "$ROOT/build/cxl_axi_sram_xelab.log" 2>&1
    set +e
    xsim axi_sim -R > "$ROOT/build/cxl_axi_sram_stdout.log" 2>&1
    RC=$?
    set -e
fi

cat "$ROOT/build/cxl_axi_sram_stdout.log"
if grep -q '^TB_CXL_AXI_DPI: ALL PASS' "$ROOT/build/cxl_axi_sram_stdout.log" &&
   grep -q '^TB_CXL_AXI_SRAM: ALL PASS' "$ROOT/build/cxl_axi_sram_stdout.log" &&
   [ $RC -eq 0 ]; then
    echo "[TB-CXL-AXI-DPI] PASS ($SIM)"
else
    echo "[TB-CXL-AXI-DPI] FAIL (rc=$RC, SIM=$SIM)"
    exit 1
fi
