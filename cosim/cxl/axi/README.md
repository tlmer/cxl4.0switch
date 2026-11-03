# CXL model AXI4 host-DDR interface

This directory is the pin-level integration of the model's optional 128-bit
AXI4 master with an external SRAM that represents motherboard DDR.

```
C++ model (CxlRoot / CxlCacheHome)
        |  CxlAxiMaster::write/read (16-byte beats, 4 KiB max burst)
        v
DpiAxiBackend (cxl_axi_dpi_link.cpp)
        |  cxl_axi_model_poll / wdata / rdata / done  (SV -> C++ imports)
        v
cxl_axi_dpi_master.sv  -- AXI4 128-bit -->  axi_ram_ref.v (1 MiB SRAM)
                                              ^
                        tb_cxl_axi_sram.sv ---+ checks the SRAM contents
```

## Contract

| Item | Value |
|---|---|
| Data / address / ID | 128 bit / 64 bit / 4 bit |
| Outstanding (model table) | 8 |
| Burst | AXI4 INCR, 1..256 beats (4 KiB) |
| Unaligned access | align down to 16 B, select bytes with WSTRB |
| Backend | functional SRAM (`CxlAxiSramBackend`) or DPI/SV pins |

The model thread blocks in `CxlAxiMaster::read/write`; the simulator thread
services the bus.  This is the same threading model as the model-host DPI example.

## Run

```sh
sh cosim/cxl/axi/run.sh
```

The script builds `build/libcxl_axi_dpi.so`, compiles the SV bench with VCS,
runs it, and requires:

```text
TB_CXL_AXI_DPI: ALL PASS (n/n)
TB_CXL_AXI_SRAM: ALL PASS (n/n)
```

## Integration boundary

- The AXI4 port is an **optional host-memory attachment**, not a replacement
  for the CXL/PCIe protocol boundary.
- `axi_ram_ref.v` is the customer-style SRAM reference kept locally so the
  benchmark is self-contained; the customer tree is never modified.
- The DPI engine currently serializes bursts (one active AXI request) while the
  model-side table retains 8 outstanding descriptors.  A multi-active SV engine
  can be added without changing the model API.
