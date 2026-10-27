#!/usr/bin/env python3
"""Check the model port adapter against the customer swdn_cxl DUT top.

The adapter (`rtl_port/cxl_swdn_port_if.sv`) is a pin map for one customer
swdn instance.  This checker parses both Verilog modules and verifies that
every DUT app-side port (PHY serial pins excluded) exists in the adapter with
the same direction and bit width.

Terminal line: CXL_DUT_PORT_CHECK: ALL PASS (n/n)
"""
import os
import re
import sys

ROOT = os.environ.get("CXL3SW_HOME", "/mnt/hgfs/g/cxl_sw33")
DUT = os.path.join(ROOT, "top1", "swdn_cxl.v")
ADAPTER = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                       "rtl_port", "cxl_swdn_port_if.sv")
EXCLUDE = {"pcie4rxp", "pcie4rxn", "pcie4txp", "pcie4txn",
           "pcie4_clk_p", "pcie4_clk_n"}

# 68B customer build constants (CXL3SW config).
PARAMS = {
    "CX_NFUNC": 1,
    "CX_LBC_NW": 2,
    "CX_LBC_EXT_AW": 32,
    "CDC_FIFOWIDTH": 771,
    "CX_CXL_CACHE_H2D_REQ_IFNUM": 1, "CX_CXL_CACHE_H2D_REQ_SIZE": 64,
    "CX_CXL_CACHE_H2D_RESP_IFNUM": 1, "CX_CXL_CACHE_H2D_RESP_SIZE": 32,
    "CX_CXL_CACHE_H2D_DATA_IFNUM": 1, "CX_CXL_CACHE_H2D_DATA_SIZE": 536,
    "CX_CXL_CACHE_D2H_REQ_IFNUM": 1, "CX_CXL_CACHE_D2H_REQ_SIZE": 79,
    "CX_CXL_CACHE_D2H_RESP_IFNUM": 1, "CX_CXL_CACHE_D2H_RESP_SIZE": 20,
    "CX_CXL_CACHE_D2H_DATA_IFNUM": 1, "CX_CXL_CACHE_D2H_DATA_SIZE": 593,
    "CX_CXL_MEM_M2S_NDR_IFNUM": 1, "CX_CXL_MEM_M2S_NDR_SIZE": 87,
    "CX_CXL_MEM_M2S_DATA_IFNUM": 1, "CX_CXL_MEM_M2S_DATA_SIZE": 663,
    "CX_CXL_MEM_S2M_NDR_IFNUM": 1, "CX_CXL_MEM_S2M_NDR_SIZE": 30,
    "CX_CXL_MEM_S2M_DATA_IFNUM": 1, "CX_CXL_MEM_S2M_DATA_SIZE": 552,
}


def parse_ports(path, module):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    text = re.sub(r"//.*", "", text)
    m = re.search(r"module\s+%s\b.*?\)\s*\((.*?)\);" % module, text, re.S)
    if not m:
        raise SystemExit("module %s not found in %s" % (module, path))
    body = m.group(1)
    ports = []
    pat = re.compile(
        r"(input|output|inout)\s+(?:(?:logic|wire|reg)\s+)?"
        r"(?:\[([^\]]*)\]\s*)?(.*?)(?=(?:input|output|inout)\s|$)", re.S)
    for mm in pat.finditer(body):
        direction, rng, names = mm.group(1), mm.group(2), mm.group(3)
        for name in [x.strip() for x in names.split(",") if x.strip()]:
            name = name.split()[-1]
            if not re.match(r"^[A-Za-z_]\w*$", name):
                continue
            ports.append((name, direction, rng or ""))
    return ports


def width_of(rng, local):
    if not rng:
        return 1
    lhs = rng.split(":")[0].strip()
    expr = lhs
    for key, val in local.items():
        expr = expr.replace(key, str(val))
    expr = re.sub(r"`([A-Za-z_]\w*)",
                  lambda m: str(PARAMS.get(m.group(1), 0)), expr)
    for key in sorted(PARAMS, key=len, reverse=True):
        expr = expr.replace(key, str(PARAMS[key]))
    if not re.match(r"^[0-9+\-*/() ]+$", expr):
        raise ValueError("cannot evaluate width expression %r" % rng)
    return int(eval(expr, {"__builtins__": {}}, {})) + 1


def main():
    dut_ports = [(n, d, r) for n, d, r in parse_ports(DUT, "swdn_cxl")
                 if n not in EXCLUDE]
    adapter_ports = parse_ports(ADAPTER, "cxl_swdn_port_if")
    # adapter parameter defaults
    with open(ADAPTER, "r", encoding="utf-8", errors="replace") as f:
        atext = f.read()
    local = {}
    for m in re.finditer(r"parameter\s+int\s+(\w+)\s*=\s*(\d+)", atext):
        local[m.group(1)] = int(m.group(2))
    amap = {n: (d, r) for n, d, r in adapter_ports}

    ok = 0
    bad = 0
    for name, direction, rng in dut_ports:
        if name not in amap:
            print("[FAIL] missing adapter port: %s (%s [%s])" %
                  (name, direction, rng or "1"))
            bad += 1
            continue
        adir, arng = amap[name]
        if adir != direction:
            print("[FAIL] direction %s: DUT %s, adapter %s" %
                  (name, direction, adir))
            bad += 1
            continue
        try:
            dw = width_of(rng, {})
            aw = width_of(arng, local)
        except ValueError as exc:
            print("[FAIL] width parse %s: %s" % (name, exc))
            bad += 1
            continue
        if dw != aw:
            print("[FAIL] width %s: DUT %d, adapter %d" %
                  (name, dw, aw))
            bad += 1
            continue
        ok += 1

    # Adapter-only ports are fine if they are explicit model-side additions.
    for name, direction, rng in adapter_ports:
        if name not in {x[0] for x in dut_ports}:
            print("[INFO] adapter-only port: %s (%s [%s])" %
                  (name, direction, rng or "1"))

    print("CXL_DUT_PORT_CHECK: %s (%d/%d)" %
          ("ALL PASS" if bad == 0 else "FAIL", ok, ok + bad))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
