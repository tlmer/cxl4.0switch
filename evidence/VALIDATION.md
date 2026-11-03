# Validation Summary

Model-side verification completed 2026-10-30 on Linux x86_64 with g++ 8.5.
**2026-11-03 update**: SystemC migration (N8/SC1-SC5) added; see the
"N8 SystemC snapshot" section below.  The last *full* tree regression is
still the 2026-10-30 run (84 gates / 57 CXL gates); the SC gates listed
below were validated individually while the final full regression is
pending before the next publish.

| Gate | Result |
|---|---|
| Feature testbenches | 44 testbenches / 600+ checks, ALL PASS (each with red/negative checks) |
| Parent PCIe baseline | `pcie6sw` 28/28 PASS, EXIT=0 |
| CXL full regression | `cxl6sw` 84 PASS gates / 0 FAIL / RUN_RC=0 (57 CXL gates) |
| PCIe base + EP/cascaded enumeration | `TB-CXL-ENUM: ALL PASS (44/44)`; E6 Type0 EP, E7 Type1 cascade, E8 cascade HDM/data |
| Post-E8 delta | ENUM 44/44; SWITCH 13/13; HDM-SEQ 8/8; CASCADE 10/10; PBR 12/12; MEM 14/14; LINUX 21/21 |
| DTS template + runtime application | `TB_CXL_DTS: ALL PASS (8/8)` |
| Testcase channel / ioctl / trace / coverage | `TB_CXL_TC: ALL PASS (19/19)`; injected-failure red build `TB_CXL_TC_RED: ALL PASS (3/3)` |
| DUT docking vectors | `TB_CXL_DUT_DOCKING: ALL PASS (20/20)` |
| DUT port compatibility | `CXL_DUT_PORT_CHECK: ALL PASS (107/107)` |
| Force manifest | 94 force statements classified; per-DUT subsets generated |
| Console | `--run-all` PASS; `--tc all` PASS (18 cases) |
| Independent library smoke | `SMOKE_CXL: ALL PASS` using `include/` + `lib/` only |
| Delivery library | 36 objects / 40 public headers; English and path-leak gates clean |

MODEL_TRACE classes observed in a full run: STAGE, IRQ, EXC, ERR, RESULT, SUM.

## N8 SystemC snapshot (2026-11-03)

The model upper half (root, switch USP/crossbar, N×swdn ports) has a
SystemC implementation on top of the same U-layer objects; the released
plain-C++ library remains SystemC-free.

| SC gate | Result |
|---|---|
| `TB-CXL-SC-SMOKE` (root + switch + N swdn, standard hierarchy) | 84 modes / 0 FAIL (last full smoke run) |
| `TB-CXL-SC-CHAN` (10-channel agent; init/credit/data/DUT vectors) | green 20/20 · chan_data 18/18 · dut_vectors 24/24 · 3 red |
| `TB-CXL-SC-CHAN-HOST` (cache/mem/snoop/poison semantics) | 26/26 + 3 red |
| `TB-CXL-SC-CHAN-BANK` (N=1/2/4/8, BC/MC) | green + 2 red |
| `TB-CXL-SC-MEMBRIDGE` / `SWITCH-MEM` (PrivTlp→M2S, CplD) | 7/7 per profile (2/4/8) |
| `TB-CXL-SC-PBR-DATA` (RGT routing + G-FAM) | 6/6 + 3/3 + 3/3 |
| `TB-CXL-SC-CACHE-LRU` (device-level LRU/writeback) | 6/6 + 4/4 + 1/1 |
| `TB-CXL-SC-VPPB-FULL` (Type1/rails/extcaps/D3hot) | 10/7/6/4/3 |
| `TB-CXL-SC-SYSTEM` (final single hierarchy, N=2/4/8) | 8/8 · 8/8 · 10/10 · 10/10 · 10/10 · 4/4 (incl. MC/BC) |
| `TB_CXL_SC_DUT` (real swdn pair DUT via SC channel agent, VCS SystemC) | 5/5 (link up @99.56 µs, init, H2D/M2S crossing) |

P1–P5 SC coverage matrix: **27 / 0 / 0** (full / partial / pending).
The final full regression and the W5 model-host co-simulation re-run are
pending; they are required before the next public publish.

## Delivery update (2026-11-03)

`build_cxl_lib.sh` (plain-C++ delivery): **39 objects / 43 public headers**;
English, path-leak and vendor-name gates clean.  `stage_repo.sh` reruns the
independent smoke/console against the delivered library:
`SMOKE_CXL: ALL PASS`, console `18/18`.  SystemC module headers/sources
(`cxl_sc_*`) are deliberately excluded from this SystemC-free archive.

## Pending / SKIP (not counted as pass)

- Customer `swdn` + EP DUT RTL co-simulation (M6.2-M6.7) and the full DPI
  bridge: pending customer environment.  Model-side docking vectors, port
  check, force subsets and wrapper are complete.
- Local model-to-DUT bring-up (clean standard controller + the supplied
  endpoint wrapper) passes PCIe link-up, CXL.io configuration reads, CXL.mem
  M2S/S2M traffic, and DBI configuration through the port adapter; this is an
  internal integration result and does not replace customer-environment
  co-simulation.
- Customer RTL PBR/256B profile: the delivered CTL configuration has PBR
  disabled; the model spec profile is validated.
- PHY electrical/link training/CRC/retry, physical media poison path,
  vendor-specific slot power and GIM internal GFD mapping.
- NTB and DM are minimal model-defined scope (F1a/F1b); not full vendor APIs.
- Cascaded-switch data-plane HDM is model-verified through `mem_route_backend`
  and a model endpoint backend; customer cascaded RTL data-plane integration
  remains part of the customer RTL co-simulation (environment pending).

Raw evidence logs are retained in the internal build tree; the release gate
reruns the staged smoke and console against the delivered library.
