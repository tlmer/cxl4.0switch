// -----------------------------------------------------------------------------
// cxl_uvm_ctl.h -- simple ioctl-style control surface for the CXL UVM TB.
//
//   The UVM testbench calls cxl_uvm_ctl_poll() every 1 ms.  A user process
//   connects to a Unix-domain socket and sends one command per connection:
//
//     LIST
//     RUN <id|all> [iterations]
//     COVERAGE
//     QUIT
//
//   This mirrors the existing cxl_tc_ref.h ioctl ABI (GET_COUNT/GET_INFO/RUN/
//   COVERAGE); a kernel character device can forward its unlocked_ioctl() to
//   the same dispatcher.  The socket transport is deliberately simple so the
//   UVM TB has no extra UVM components.
// -----------------------------------------------------------------------------
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Return codes used by cxl_uvm_ctl_poll().
enum {
    CXL_UVM_CTL_NONE = 0,
    CXL_UVM_CTL_LIST = 1,
    CXL_UVM_CTL_RUN = 2,
    CXL_UVM_CTL_COVERAGE = 3,
    CXL_UVM_CTL_QUIT = 4
};

// Create the listening socket.  Path defaults to /tmp/cxl_uvm_ctl.sock when
// null/empty.  Returns 0 on success.
int cxl_uvm_ctl_init(const char* path);

// Poll for one complete command.  Returns one of CXL_UVM_CTL_* (0 = none).
// For RUN, id is the testcase index, or -1 for ALL, and iterations is >= 1.
int cxl_uvm_ctl_poll(int* id, int* iterations);

// Execute a polled command and send the response to the connected client.
// Every non-zero poll result must be completed with exactly one of these.
void cxl_uvm_ctl_list(void);
void cxl_uvm_ctl_run(int id, int iterations);
void cxl_uvm_ctl_coverage(void);
void cxl_uvm_ctl_quit(void);

// Close/unlink the socket.
void cxl_uvm_ctl_close(void);

#ifdef __cplusplus
}
#endif
