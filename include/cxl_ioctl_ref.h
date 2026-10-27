// -----------------------------------------------------------------------------
// cxl_ioctl_ref.h -- ioctl reference ABI for the CXL Linux proxy (checklist #6).
//
//   These are model-side commands for a user-space UVM/validation process.  They
//   are deliberately separate from the standard testcase channel
//   (cxl_tc_ref.h, magic 't') so a customer's DUT UAPI stays untouched.
//
//   All structures are fixed-width and naturally aligned.  Strings are
//   NUL-terminated.  Unknown requests return -ENOTTY (never silently ignored).
// -----------------------------------------------------------------------------
#pragma once
#include <stdint.h>

#if defined(__linux__)
#include <sys/ioctl.h>
#else
// Minimal fallback for non-Linux hosts: the numeric ABI is still stable.
#define _IOC(dir, type, nr, size) \
    (((dir) << 30) | ((type) << 8) | (nr) | ((size) << 16))
#define _IO(type, nr) _IOC(0, (type), (nr), 0)
#define _IOR(type, nr, size) _IOC(2, (type), (nr), sizeof(size))
#define _IOW(type, nr, size) _IOC(1, (type), (nr), sizeof(size))
#define _IOWR(type, nr, size) _IOC(3, (type), (nr), sizeof(size))
#endif

#define CXL_IOC_MAGIC 'C'
#define CXL_IOC_VERSION 0x00010000u

struct cxl_ioc_info {
    uint32_t version;          // CXL_IOC_VERSION
    uint32_t num_ports;        // 1..32
    uint32_t num_devices;      // discovered Type-3 devices
    uint32_t num_regions;      // created HDM regions
    uint32_t flit_mode;        // CxlFlitMode value
    uint32_t mailbox_payload;  // bytes
    uint32_t dpa_bytes;        // per-device DPA size used by the proxy
    uint32_t caps;             // bit0 PBR, bit1 256B, bit2 cache, bit3 MLD
};

struct cxl_ioc_enum {
    uint32_t device_count;     // out
    uint32_t region_count;     // out
    uint32_t ok;               // out: 1 = enumeration and HDM programming passed
    uint32_t reserved;
};

struct cxl_ioc_cfg {
    uint32_t port;             // DSP index
    uint32_t offset;           // configuration-space byte offset
    uint32_t size;             // 1, 2, or 4
    uint32_t value;            // in/out
};

struct cxl_ioc_state {
    uint64_t reads;            // CXL.mem operations submitted by the proxy
    uint64_t writes;
    uint64_t irq_asserts;      // CEDT/event/poison notification count
    uint64_t mailbox_commands;
    uint64_t errors;           // sum of model error counters
    uint32_t ports;
    uint32_t regions;
    uint32_t ready;
    uint32_t reserved;
};

struct cxl_ioc_perf {
    uint32_t kind;             // in: 0 = CXL.mem path, 1 = configuration path
    uint32_t reserved;
    uint64_t ops;
    uint64_t bytes;
    uint64_t window_ns;
    uint64_t latency_min_ns;
    uint64_t latency_avg_ns;
    uint64_t latency_max_ns;
    double   kb_per_s;
    double   kops_per_s;
};

#define CXL_IOC_GET_INFO  _IOR(CXL_IOC_MAGIC, 0x01, struct cxl_ioc_info)
#define CXL_IOC_ENUM      _IOWR(CXL_IOC_MAGIC, 0x02, struct cxl_ioc_enum)
#define CXL_IOC_CFG_RD    _IOWR(CXL_IOC_MAGIC, 0x03, struct cxl_ioc_cfg)
#define CXL_IOC_CFG_WR    _IOW(CXL_IOC_MAGIC, 0x04, struct cxl_ioc_cfg)
#define CXL_IOC_GET_STATE _IOR(CXL_IOC_MAGIC, 0x05, struct cxl_ioc_state)
#define CXL_IOC_GET_PERF  _IOWR(CXL_IOC_MAGIC, 0x06, struct cxl_ioc_perf)
#define CXL_IOC_RESET_PERF _IOW(CXL_IOC_MAGIC, 0x07, struct cxl_ioc_perf)
