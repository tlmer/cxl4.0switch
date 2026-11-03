// -----------------------------------------------------------------------------
// cxl_uvm_tb.sv -- small UVM testbench with an ioctl-style control surface.
//
//   The UVM run_phase polls /tmp/cxl_uvm_ctl.sock every 1 ms.  A user process
//   (cxl_uvm_ctl CLI, or any Unix-socket client) sends:
//
//     LIST
//     RUN <id|all> [iterations]
//     COVERAGE
//     QUIT
//
//   RUN/COVERAGE execute the CXL model testcase registry (cxl_tc) through the
//   DPI shim; the selected tests are the same cases the delivered library
//   exposes.  This is intentionally a small TB: it is a control/orchestration
//   shell, not another RTL testbench.
// -----------------------------------------------------------------------------
`timescale 1ns/1ps

import uvm_pkg::*;
`include "uvm_macros.svh"

import "DPI-C" context function int  cxl_uvm_ctl_init(input string path);
import "DPI-C" context function int  cxl_uvm_ctl_poll(output int id, output int iterations);
import "DPI-C" context function void cxl_uvm_ctl_list();
import "DPI-C" context function void cxl_uvm_ctl_run(int id, int iterations);
import "DPI-C" context function void cxl_uvm_ctl_coverage();
import "DPI-C" context function void cxl_uvm_ctl_quit();
import "DPI-C" context function void cxl_uvm_ctl_close();

class cxl_uvm_ioctl_test extends uvm_test;
    `uvm_component_utils(cxl_uvm_ioctl_test)

    function new(string name = "cxl_uvm_ioctl_test", uvm_component parent = null);
        super.new(name, parent);
    endfunction

    task run_phase(uvm_phase phase);
        int cmd, id, iterations;
        int rc;
        phase.raise_objection(this);

        rc = cxl_uvm_ctl_init("/tmp/cxl_uvm_ctl.sock");
        if (rc != 0) begin
            `uvm_fatal("CXL_UVM_IOCTL",
                       $sformatf("socket init failed rc=%0d (is /tmp writable?)", rc))
        end
        `uvm_info("CXL_UVM_IOCTL",
                  "READY: use cxl_uvm_ctl list|run|coverage|quit", UVM_LOW)

        forever begin
            #(1ms);
            cmd = cxl_uvm_ctl_poll(id, iterations);
            case (cmd)
                1: cxl_uvm_ctl_list();
                2: cxl_uvm_ctl_run(id, iterations);
                3: cxl_uvm_ctl_coverage();
                4: begin
                       cxl_uvm_ctl_quit();
                       cxl_uvm_ctl_close();
                       `uvm_info("CXL_UVM_IOCTL", "QUIT received; ending UVM TB", UVM_LOW)
                       break;
                   end
                default: ; // no command pending
            endcase
        end

        phase.drop_objection(this);
    endtask
endclass

module cxl_uvm_tb_top;
    initial begin
        run_test("cxl_uvm_ioctl_test");
    end
endmodule
