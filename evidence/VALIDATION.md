# Validation Summary

Model-side verification completed 2026-10-27 on Linux x86_64 with g++ 8.5.

| Gate | Result |
|---|---|
| Feature testbenches | 44 testbenches / 600 checks, ALL PASS (each with red/negative checks) |
| Parent PCIe baseline | `pcie6sw` 28/28 PASS, EXIT=0 |
| CXL full regression | `cxl6sw` 78 PASS gates / 0 FAIL / EXIT=0 (52 CXL gates) |
| DTS template + runtime application | `TB_CXL_DTS: ALL PASS (8/8)` |
| Testcase channel / ioctl / trace / coverage | `TB_CXL_TC: ALL PASS (19/19)`; injected-failure red build `TB_CXL_TC_RED: ALL PASS (3/3)` |
| DUT docking vectors | `TB_CXL_DUT_DOCKING: ALL PASS (20/20)` |
| DUT port compatibility | `CXL_DUT_PORT_CHECK: ALL PASS (107/107)` |
| Force manifest | 94 force statements classified; per-DUT subsets generated |
| Console | `--run-all` PASS; `--tc all` PASS (18 cases) |
| Independent library smoke | `SMOKE_CXL: ALL PASS` using `include/` + `lib/` only |
| Delivery library | 31 objects / 36 public headers; English and path-leak gates clean |

MODEL_TRACE classes observed in a full run: STAGE, IRQ, EXC, ERR, RESULT, SUM.

## Pending / SKIP (not counted as pass)

- Customer `swdn` + EP DUT RTL co-simulation (M6.2-M6.7) and the full DPI
  bridge: pending customer environment.  Model-side docking vectors, port
  check, force subsets and wrapper are complete.
- Customer RTL PBR/256B profile: the delivered CTL configuration has PBR
  disabled; the model spec profile is validated.
- PHY electrical/link training/CRC/retry, physical media poison path,
  vendor-specific slot power and GIM internal GFD mapping.
- NTB and DM are out of scope.

Raw evidence logs are retained in the internal build tree; the release gate
reruns the staged smoke and console against the delivered library.
