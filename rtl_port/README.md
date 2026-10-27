# CXL grouped port abstraction (the only RTL delivered by the model)

`cxl_top_port_grouped.sv` presents the model/DUT boundary as **one packed
vector per standard controller interface face and direction**.  The source is
the standard controller top port list, not a customer wrapper.  Local or
implementation-specific faces (RAM, PIPE/PHY, retry buffers, VPD, VMI, SII,
MSI) are intentionally excluded; the customer keeps those local.

Every group width is a **module parameter** with a 68B default.  For a different
customer configuration override the parameters; the private per-pin adapter,
element order and simulator portmap are generated for that configuration and
are not part of this public package.

| File | Purpose |
|---|---|
| `cxl_top_port_grouped.sv` | Parameterized grouped interface module (no functional logic) |
| `cxl_top_port_grouped.veo` | Instantiation template |

## Groups (model boundary)

| Face | Direction |
|---|---|
| `xali0_in` / `xali0_out` | CXL.io client0 TLP channel |
| `xali1_in` / `xali1_out` | CXL.io client1 (configuration) TLP channel |
| `rcpl_out` | receive completion TLP channel |
| `rtrgt1_in` / `rtrgt1_out` | target1 TLP channel and halt |
| `dbi_in` / `dbi_out` | DBI register handshake |
| `elbi_in` / `elbi_out` | external local-bus request/response |
| `cxl_msg_in` / `cxl_msg_out` | CXL message channel |
| `cxl_cache_init_rx_in/out` | cache link initialization (RX manager) |
| `cxl_cache_init_tx_in/out` | cache link initialization (TX subordinate) |
| `cxl_cache_hm_rx_in/out` | CXL.cache host-manager RX |
| `cxl_cache_ds_tx_in/out` | CXL.cache device-subordinate TX |
| `cxl_cache_hs_tx_in/out` | CXL.cache host-subordinate TX |
| `cxl_cache_dm_rx_in/out` | CXL.cache device-manager RX |
| `cxl_mem_init_rx_in/out` | mem link initialization (RX manager) |
| `cxl_mem_init_tx_in/out` | mem link initialization (TX subordinate) |
| `cxl_mem_hm_rx_in/out` | CXL.mem host-manager RX |
| `cxl_mem_ds_tx_in/out` | CXL.mem device-subordinate TX |
| `cxl_mem_hs_tx_in/out` | CXL.mem host-subordinate TX |
| `cxl_mem_dm_rx_in/out` | CXL.mem device-manager RX |
| `cxl_mgmt_out` | CXL error/DVSEC/register-locator status |
| `clk_rst_in` / `clk_rst_out` | application clocks/resets (test/PIPE members excluded) |

## Protocol rules

1. CXL.cachemem has no ready/halt: a valid beat is consumed when it appears.
2. Credit values are per channel and must not be over-drawn.
3. Init handshake follows req/ack; deinit is explicit.
4. DBI: `cs` remains asserted until `ack`; `ack` is exactly two cycles.
5. Do not change a grouped vector between a valid beat and its acceptance.
6. The private adapter must preserve the standard face ordering; do not
   reorder elements when changing the customer configuration.

## Bring-up forces

The customer root testbench contains testbench-only `initial force` statements
that enable the CXL ports and accelerate link training.  A co-simulation
harness must include them (or issue equivalent DBI writes).  Model criteria
must not depend on a force to pass.
