# Validation Summary

Model-side verification completed 2026-11-10 on Linux x86_64 with g++ 8.5.
**2026-11-10 update**: the complete model tree regression passed
(**115/115 gates, 0 FAIL, `G1_RC=0`**) together with the 43-bench
capability gap suite; the SystemC gates run inside the same tree.  See the
"N8 SystemC snapshot" section below.

| Gate | Result |
|---|---|
| Feature testbenches | 43-bench capability gap suite (`MODEL_GAP_TESTS: PASS`) inside the 115-gate tree; each bench also has red/negative checks |
| Parent PCIe baseline | `pcie6sw` 28/28 PASS, EXIT=0 |
| CXL full regression | `cxl6sw` **115/115 gates PASS / 0 FAIL / `G1_RC=0`** (2026-11-10) |
| PCIe base + EP/cascaded enumeration | `TB-CXL-ENUM: ALL PASS (44/44)`; E6 Type0 EP, E7 Type1 cascade, E8 cascade HDM/data |
| Post-E8 delta | ENUM 44/44; SWITCH 13/13; HDM-SEQ 8/8; CASCADE 10/10; PBR 12/12; MEM 14/14; LINUX 21/21 |
| DTS template + runtime application | `TB_CXL_DTS: ALL PASS (11/11)` |
| Testcase channel / ioctl / trace / coverage | `TB_CXL_TC: ALL PASS (19/19)`; injected-failure red build `TB_CXL_TC_RED: ALL PASS (3/3)` |
| DUT docking vectors | `TB_CXL_DUT_DOCKING: ALL PASS (20/20)` |
| DUT port compatibility | `CXL_DUT_PORT_CHECK: ALL PASS (107/107)` |
| Force manifest | 94 force statements classified; per-DUT subsets generated |
| Console | `--run-all` PASS; `--tc all` PASS (18 cases) |
| Independent library smoke | `SMOKE_CXL: ALL PASS` using `include/` + `lib/` only |
| Delivery library | 51 objects / 83 public headers (82 CXL + `pcie_ext_caps.h`); English, path-leak and vendor-name gates clean |

MODEL_TRACE classes observed in a full run: STAGE, IRQ, EXC, ERR, RESULT, SUM.

## N8 SystemC snapshot (2026-11-10)

The model upper half (root, switch USP/crossbar, N×swdn ports) has a
SystemC implementation on top of the same U-layer objects; the released
plain-C++ library remains SystemC-free.

| SC gate | Result |
|---|---|
| `TB-CXL-SC-SMOKE` (root + switch + N swdn, standard hierarchy) | **93 modes / 0 FAIL** |
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
All SC gates above passed inside the 2026-11-10 115-gate full-tree run.
The model-host role was additionally exercised against an external device
VIP in a separate co-simulation flow (CXL.cache read/write-pull, CXL.mem,
and BI response injection); that flow is internal and is not part of this
SystemC-free archive.

## Delivery update (2026-11-10)

`build_cxl_lib.sh` (plain-C++ delivery): **51 objects / 83 public headers**
(82 CXL + `pcie_ext_caps.h`); English, path-leak and vendor-name gates
clean.  `stage_repo.sh` reruns the independent smoke/console against the
delivered library: **`SMOKE_CXL: ALL PASS`** (proxy + perf + 18 testcases),
console **18/18**.  SystemC module headers/sources (`cxl_sc_*`) are
excluded from this SystemC-free archive.  The LTR/PRI/PTM data-plane
helper headers include the SystemC `pcie_sc_wire.h`, so only their
capability-register surface ships (in `pcie_ext_caps.h`); the data-plane
helpers remain internal to the SystemC layer.

## Pending / SKIP (not counted as pass)

- Customer `swdn` + EP DUT RTL co-simulation (M6.2-M6.7) and the full DPI
  bridge: pending customer environment.  Model-side docking vectors, port
  check, force subsets and wrapper are complete.
- Local model-to-DUT bring-up (clean standard controller + the supplied
  endpoint wrapper) passes PCIe link-up, CXL.io configuration reads, CXL.mem
  M2S/S2M traffic, and DBI configuration through the port adapter.  A
  four-port version (four `swdn` RTL cores + four RTL EPs) passes
  MC/BC/MemRd M1-M5 **14/14** and the K12-3/K12-7 checks; this is an
  internal integration result and does not replace customer-environment
  co-simulation.
- Customer RTL PBR/256B profile: the delivered CTL configuration has PBR
  disabled; the model spec profile is validated.
- PHY electrical/link training/CRC/retry, physical media poison path,
  vendor-specific slot power and GIM internal GFD mapping.
- NTB and DM are minimal model-defined scope (F1a/F1b); not full vendor APIs.
- Model-side known limits (not defects): CXL.cache TEE and vendor-specific
  replacement policies; remaining PCIe extended-capability registers in the
  vPPB read 0; DCD is opt-in (no multi-host/tag sharing, media/scan are owner
  hooks, with 1..8-host/tag sharing implemented); Security Send/Receive is
  an owner passthrough (TCG/SED not modeled); IDE key/stream/IV and
  AES-256-GCM are modeled with an owner cipher hook (real RTL link framing/
  TSP is customer-owned); DOE has Discovery + CDAT + optional IDE_KM (CMA
  rejected); CPMU RegLoc mapping/MSI-X integration
  is platform-owned; VPD is a register window (ROM contents are a separate
  profile); LTR/VDM/PTM multi-DW is supported by the C++ model but the W5
  host agent is limited to one DW.
- Cascaded-switch data-plane HDM is model-verified through `mem_route_backend`
  and a model endpoint backend; customer cascaded RTL data-plane integration
  remains part of the customer RTL co-simulation (environment pending).

Raw evidence logs are retained in the internal build tree; the release gate
reruns the staged smoke and console against the delivered library.
