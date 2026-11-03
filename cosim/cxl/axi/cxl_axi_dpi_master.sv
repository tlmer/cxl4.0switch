// =============================================================================
// cxl_axi_dpi_master.sv -- 128-bit AXI4 master driven by the CXL model.
//
//   The C++ model owns the host memory view.  This bridge is the only piece on
//   the pin side: it fetches one burst descriptor from the model, drives the
//   AXI4 master channels, and returns read beats / the final response.
//
//   Contract: DW=128, AW=64, AxID=4, INCR bursts, up to 256 beats.  The
//   external SRAM (axi_ram) is the motherboard DDR; it is instantiated by the
//   testbench, not by the model.
//
//   DPI direction (all functions below are SV -> C++ imports):
//     cxl_axi_model_poll   fetch a burst
//     cxl_axi_model_wdata  pull one 128-bit write beat + 16 byte strobes
//     cxl_axi_model_rdata  push one 128-bit read beat
//     cxl_axi_model_done   complete the burst
// =============================================================================
`timescale 1ns/1ps

module cxl_axi_dpi_master #(
    parameter integer AW    = 64,
    parameter integer DW    = 128,
    parameter integer IDW   = 4,
    parameter integer BEATS = 256
) (
    input  wire             clk,
    input  wire             rst_n,

    // AXI4 master
    output logic [IDW-1:0]  m_awid,
    output logic [AW-1:0]   m_awaddr,
    output logic [7:0]      m_awlen,
    output logic [2:0]      m_awsize,
    output logic [1:0]      m_awburst,
    output logic            m_awvalid,
    input  wire             m_awready,

    output logic [DW-1:0]   m_wdata,
    output logic [DW/8-1:0] m_wstrb,
    output logic            m_wlast,
    output logic            m_wvalid,
    input  wire             m_wready,

    input  wire [IDW-1:0]   m_bid,
    input  wire [1:0]       m_bresp,
    input  wire             m_bvalid,
    output wire             m_bready,

    output logic [IDW-1:0]  m_arid,
    output logic [AW-1:0]   m_araddr,
    output logic [7:0]      m_arlen,
    output logic [2:0]      m_arsize,
    output logic [1:0]      m_arburst,
    output logic            m_arvalid,
    input  wire             m_arready,

    input  wire [IDW-1:0]   m_rid,
    input  wire [DW-1:0]    m_rdata,
    input  wire [1:0]       m_rresp,
    input  wire             m_rlast,
    input  wire             m_rvalid,
    output wire             m_rready
);

    import "DPI-C" function void cxl_axi_model_poll(
        output int valid,
        output longint unsigned addr,
        output int size,
        output int beats,
        output int id,
        output int is_write,
        output longint unsigned handle);
    import "DPI-C" function void cxl_axi_model_wdata(
        input longint unsigned handle,
        input int beat,
        output longint unsigned data_lo,
        output longint unsigned data_hi,
        output longint unsigned strb_lo,
        output longint unsigned strb_hi);
    import "DPI-C" function void cxl_axi_model_rdata(
        input longint unsigned handle,
        input int beat,
        input longint unsigned data_lo,
        input longint unsigned data_hi);
    import "DPI-C" function void cxl_axi_model_done(
        input longint unsigned handle,
        input int resp);

    localparam [2:0] S_IDLE = 3'd0, S_AW = 3'd1, S_W = 3'd2,
                     S_B    = 3'd3, S_AR = 3'd4, S_R = 3'd5;

    reg [2:0]               state;
    reg [AW-1:0]            cur_addr;
    reg [8:0]               cur_beats;       // 1..256
    reg [IDW-1:0]           cur_id;
    reg [8:0]               beat;
    reg                     cur_write;
    reg [63:0]              cur_handle;

    int                     dpi_valid, dpi_size, dpi_beats, dpi_id, dpi_write;
    longint unsigned        dpi_addr, dpi_handle;
    longint unsigned        dpi_dlo, dpi_dhi, dpi_slo, dpi_shi;
    longint unsigned        dpi_rlo, dpi_rhi;

    assign m_bready = 1'b1;
    assign m_rready = 1'b1;

    function automatic void load_wbeat(input int b);
        cxl_axi_model_wdata(cur_handle, b, dpi_dlo, dpi_dhi, dpi_slo, dpi_shi);
        m_wdata <= {dpi_dhi, dpi_dlo};
        m_wstrb <= {dpi_shi[7:0], dpi_slo[7:0]};
        m_wlast <= (b == (dpi_beats - 1));
    endfunction

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state       <= S_IDLE;
            m_awvalid   <= 1'b0;
            m_wvalid    <= 1'b0;
            m_arvalid   <= 1'b0;
            m_awid      <= '0;
            m_awaddr    <= '0;
            m_awlen     <= 8'd0;
            m_awsize    <= 3'd4;
            m_awburst   <= 2'b01;
            m_wdata     <= '0;
            m_wstrb     <= '0;
            m_wlast     <= 1'b0;
            m_arid      <= '0;
            m_araddr    <= '0;
            m_arlen     <= 8'd0;
            m_arsize    <= 3'd4;
            m_arburst   <= 2'b01;
            cur_addr    <= '0;
            cur_beats   <= 9'd0;
            cur_id      <= '0;
            cur_write   <= 1'b0;
            cur_handle  <= '0;
            beat        <= 9'd0;
        end else begin
            case (state)
            S_IDLE: begin
                cxl_axi_model_poll(dpi_valid, dpi_addr, dpi_size, dpi_beats,
                                   dpi_id, dpi_write, dpi_handle);
                if (dpi_valid != 0) begin
                    cur_addr   <= dpi_addr[AW-1:0];
                    cur_beats  <= dpi_beats[8:0];
                    cur_id     <= dpi_id[IDW-1:0];
                    cur_write  <= (dpi_write != 0);
                    cur_handle <= dpi_handle;
                    beat       <= 9'd0;
                    if (dpi_write != 0) begin
                        m_awid    <= dpi_id[IDW-1:0];
                        m_awaddr  <= dpi_addr[AW-1:0];
                        m_awlen   <= dpi_beats[7:0] - 8'd1;
                        m_awsize  <= dpi_size[2:0];
                        m_awburst <= 2'b01;
                        m_awvalid <= 1'b1;
                        state     <= S_AW;
                    end else begin
                        m_arid    <= dpi_id[IDW-1:0];
                        m_araddr  <= dpi_addr[AW-1:0];
                        m_arlen   <= dpi_beats[7:0] - 8'd1;
                        m_arsize  <= dpi_size[2:0];
                        m_arburst <= 2'b01;
                        m_arvalid <= 1'b1;
                        state     <= S_AR;
                    end
                end
            end
            S_AW: begin
                if (m_awvalid && m_awready) begin
                    m_awvalid <= 1'b0;
                    load_wbeat(0);
                    m_wvalid  <= 1'b1;
                    state     <= S_W;
                end
            end
            S_W: begin
                if (m_wvalid && m_wready) begin
                    if (beat == (cur_beats - 9'd1)) begin
                        m_wvalid <= 1'b0;
                        state    <= S_B;
                    end else begin
                        beat <= beat + 9'd1;
                        load_wbeat(beat + 9'd1);
                    end
                end
            end
            S_B: begin
                if (m_bvalid) begin
                    cxl_axi_model_done(cur_handle, {30'd0, m_bresp});
                    state <= S_IDLE;
                end
            end
            S_AR: begin
                if (m_arvalid && m_arready) begin
                    m_arvalid <= 1'b0;
                    beat      <= 9'd0;
                    state     <= S_R;
                end
            end
            S_R: begin
                if (m_rvalid) begin
                    cxl_axi_model_rdata(cur_handle, beat,
                                        m_rdata[63:0], m_rdata[127:64]);
                    if (m_rlast || (beat == (cur_beats - 9'd1))) begin
                        cxl_axi_model_done(cur_handle, {30'd0, m_rresp});
                        state <= S_IDLE;
                    end else begin
                        beat <= beat + 9'd1;
                    end
                end
            end
            default: state <= S_IDLE;
            endcase
        end
    end

`ifdef CXL_AXI_DEBUG
    always @(posedge clk) begin
        if (rst_n && state != S_IDLE)
            $display("[CXL-AXI] t=%0t state=%0d h=%0h addr=%0h beat=%0d/%0d",
                     $time, state, cur_handle, cur_addr, beat, cur_beats);
    end
`endif
endmodule
