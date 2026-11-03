// =============================================================================
// tb_cxl_axi_sram.sv -- self-test for the model's 128-bit AXI4 master.
//
//   The C++ model thread issues bursts through cxl_axi_dpi_master; the AXI4
//   slave is the reference axi_ram (motherboard DDR).  After the model test
//   finishes, this bench checks that the expected bytes are physically in the
//   SRAM, not in a model-side shadow.
// =============================================================================
`timescale 1ns/1ps

module tb_cxl_axi_sram;

    import "DPI-C" function void cxl_axi_model_start();
    import "DPI-C" function int  cxl_axi_model_finished();
    import "DPI-C" function int  cxl_axi_model_failures();

    localparam integer AW  = 64;
    localparam integer DW  = 128;
    localparam integer IDW = 4;
    localparam integer RAM_AW = 20;   // 1 MiB: 65536 x 128-bit lines
    localparam integer RAM_IDW = 12;

    logic clk = 1'b0;
    logic rst_n = 1'b0;
    always #5 clk = ~clk;

    logic [IDW-1:0]   m_awid;
    logic [AW-1:0]    m_awaddr;
    logic [7:0]       m_awlen;
    logic [2:0]       m_awsize;
    logic [1:0]       m_awburst;
    logic             m_awvalid;
    logic             m_awready;
    logic [DW-1:0]    m_wdata;
    logic [DW/8-1:0]  m_wstrb;
    logic             m_wlast;
    logic             m_wvalid;
    logic             m_wready;
    logic [IDW-1:0]   m_bid;
    logic [1:0]       m_bresp;
    logic             m_bvalid;
    logic             m_bready;
    logic [IDW-1:0]   m_arid;
    logic [AW-1:0]    m_araddr;
    logic [7:0]       m_arlen;
    logic [2:0]       m_arsize;
    logic [1:0]       m_arburst;
    logic             m_arvalid;
    logic             m_arready;
    logic [IDW-1:0]   m_rid;
    logic [DW-1:0]    m_rdata;
    logic [1:0]       m_rresp;
    logic             m_rlast;
    logic             m_rvalid;
    logic             m_rready;
    logic [RAM_IDW-1:0] m_bid_ram;
    logic [RAM_IDW-1:0] m_rid_ram;

    cxl_axi_dpi_master #(.AW(AW), .DW(DW), .IDW(IDW)) u_master (
        .clk(clk), .rst_n(rst_n),
        .m_awid(m_awid), .m_awaddr(m_awaddr), .m_awlen(m_awlen),
        .m_awsize(m_awsize), .m_awburst(m_awburst), .m_awvalid(m_awvalid),
        .m_awready(m_awready),
        .m_wdata(m_wdata), .m_wstrb(m_wstrb), .m_wlast(m_wlast),
        .m_wvalid(m_wvalid), .m_wready(m_wready),
        .m_bid(m_bid), .m_bresp(m_bresp), .m_bvalid(m_bvalid),
        .m_bready(m_bready),
        .m_arid(m_arid), .m_araddr(m_araddr), .m_arlen(m_arlen),
        .m_arsize(m_arsize), .m_arburst(m_arburst), .m_arvalid(m_arvalid),
        .m_arready(m_arready),
        .m_rid(m_rid), .m_rdata(m_rdata), .m_rresp(m_rresp),
        .m_rlast(m_rlast), .m_rvalid(m_rvalid), .m_rready(m_rready)
    );

    // Motherboard DDR: the external SRAM from the customer reference shape.
    axi_ram #(
        .DATA_WIDTH(DW),
        .ADDR_WIDTH(RAM_AW),
        .STRB_WIDTH(DW/8),
        .ID_WIDTH(RAM_IDW),
        .PIPELINE_OUTPUT(0)
    ) u_ram (
        .clk(clk),
        .rst(!rst_n),
        .s_axi_awid({{(RAM_IDW-IDW){1'b0}}, m_awid}),
        .s_axi_awaddr(m_awaddr[RAM_AW-1:0]),
        .s_axi_awlen(m_awlen),
        .s_axi_awsize(m_awsize),
        .s_axi_awburst(m_awburst),
        .s_axi_awlock(1'b0),
        .s_axi_awcache(4'd0),
        .s_axi_awprot(3'd0),
        .s_axi_awvalid(m_awvalid),
        .s_axi_awready(m_awready),
        .s_axi_wdata(m_wdata),
        .s_axi_wstrb(m_wstrb),
        .s_axi_wlast(m_wlast),
        .s_axi_wvalid(m_wvalid),
        .s_axi_wready(m_wready),
        .s_axi_bid(m_bid_ram),
        .s_axi_bresp(m_bresp),
        .s_axi_bvalid(m_bvalid),
        .s_axi_bready(m_bready),
        .s_axi_arid({{(RAM_IDW-IDW){1'b0}}, m_arid}),
        .s_axi_araddr(m_araddr[RAM_AW-1:0]),
        .s_axi_arlen(m_arlen),
        .s_axi_arsize(m_arsize),
        .s_axi_arburst(m_arburst),
        .s_axi_arlock(1'b0),
        .s_axi_arcache(4'd0),
        .s_axi_arprot(3'd0),
        .s_axi_arvalid(m_arvalid),
        .s_axi_arready(m_arready),
        .s_axi_rid(m_rid_ram),
        .s_axi_rdata(m_rdata),
        .s_axi_rresp(m_rresp),
        .s_axi_rlast(m_rlast),
        .s_axi_rvalid(m_rvalid),
        .s_axi_rready(m_rready)
    );

    assign m_bid = m_bid_ram[IDW-1:0];
    assign m_rid = m_rid_ram[IDW-1:0];

    int errors = 0;
    int checks = 0;
    task automatic ck(input bit ok, input string what);
        checks++;
        if (ok) $display("  [PASS] %s", what);
        else begin
            errors++;
            $display("  [FAIL] %s", what);
        end
    endtask

    initial begin
        cxl_axi_model_start();
        #100 rst_n = 1'b1;
        begin : wait_model
            integer spins;
            for (spins = 0; spins < 2000000; spins = spins + 1) begin
                if (cxl_axi_model_finished() != 0) disable wait_model;
                #10;
            end
            $display("  [FAIL] model AXI self-test timed out");
            errors++;
        end
        #50;

        ck(cxl_axi_model_failures() == 0, "C++ model AXI self-test reported no failures");
        ck(u_ram.mem[16'h1000 >> 4][7:0] == 8'h11,
           "SRAM[0x1000] holds the model's first write byte (0x11)");
        ck(u_ram.mem[16'h2000 >> 4][7:0] == 8'h01,
           "SRAM[0x2000] holds the 4 KiB burst pattern (0x01)");
        ck(u_ram.mem[(16'h3000 + 3) >> 4][8*(16'h3003 % 16) +: 8] == 8'h30,
           "SRAM[0x3003] holds the unaligned write byte (0x30)");
        ck(u_ram.mem[16'h4000 >> 4][7:0] == 8'h44,
           "SRAM[0x4000] holds the EP dirty-evict byte (0x44)");

        $display("TB_CXL_AXI_SRAM: %s (%0d/%0d)",
                 errors == 0 ? "ALL PASS" : "FAIL", checks - errors, checks);
        $finish;
    end

endmodule
