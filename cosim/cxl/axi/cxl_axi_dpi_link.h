/* cxl_axi_dpi_link.h -- DPI interface between the CXL model and the SV AXI4
 * master engine (cxl_axi_dpi_master.sv).
 *
 * All calls are SV -> C++ imports.  The C++ model thread blocks in
 * CxlAxiMaster::read/write while the simulator thread services the bus, the
 * same threading pattern as cxl_host_dpi.cpp.
 */
#ifndef CXL_AXI_DPI_LINK_H
#define CXL_AXI_DPI_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

void cxl_axi_model_start(void);
int  cxl_axi_model_finished(void);
int  cxl_axi_model_failures(void);

/* Fetch one pending model request.  `valid` = 0 when the engine is idle. */
void cxl_axi_model_poll(int* valid,
                        unsigned long long* addr,
                        int* size,
                        int* beats,
                        int* id,
                        int* is_write,
                        unsigned long long* handle);

/* Pull one 128-bit write beat and its byte strobes. */
void cxl_axi_model_wdata(unsigned long long handle, int beat,
                         unsigned long long* data_lo,
                         unsigned long long* data_hi,
                         unsigned long long* strb_lo,
                         unsigned long long* strb_hi);

/* Push one 128-bit read beat back into the model. */
void cxl_axi_model_rdata(unsigned long long handle, int beat,
                         unsigned long long data_lo,
                         unsigned long long data_hi);

void cxl_axi_model_done(unsigned long long handle, int resp);

#ifdef __cplusplus
}
#endif

#endif /* CXL_AXI_DPI_LINK_H */
