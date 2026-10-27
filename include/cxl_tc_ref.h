// -----------------------------------------------------------------------------
// cxl_tc_ref.h -- standard testcase channel (checklist T2/T4/T5/T6).
//
//   Independent ioctl magic ('t') and ABI: the customer UAPI is not extended.
//   Typical use:
//     ioctl(fd, CXL_TC_GET_COUNT, &n);
//     ioctl(fd, CXL_TC_GET_INFO,  &info);          // info.id = i
//     ioctl(fd, CXL_TC_RUN,       &run);           // id = i or CXL_TC_ID_ALL
//                                                  // out fields: ran/passed/failed/rc
//     ioctl(fd, CXL_TC_COVERAGE,  &cov);
//
//   CXL_TC_RUN_ALL is an alias for RUN with id = CXL_TC_ID_ALL.  iterations
//   replays the same testcase in one call (T6).
// -----------------------------------------------------------------------------
#pragma once
#include <stdint.h>

#if defined(__linux__)
#include <sys/ioctl.h>
#else
#define _IOC(dir, type, nr, size) \
    (((dir) << 30) | ((type) << 8) | (nr) | ((size) << 16))
#define _IO(type, nr) _IOC(0, (type), (nr), 0)
#define _IOR(type, nr, size) _IOC(2, (type), (nr), sizeof(size))
#define _IOW(type, nr, size) _IOC(1, (type), (nr), sizeof(size))
#define _IOWR(type, nr, size) _IOC(3, (type), (nr), sizeof(size))
#endif

#define CXL_TC_MAGIC 't'
#define CXL_TC_ID_ALL 0xFFFFFFFFu
#define CXL_TC_VERSION 0x00010000u

struct cxl_tc_info {
    uint32_t id;
    uint32_t reserved;
    char name[64];
    char description[160];
};

struct cxl_tc_run {
    uint32_t id;               // in: testcase id or CXL_TC_ID_ALL
    uint32_t iterations;       // in: 0/1 = once; N > 1 replays N times (T6)
    uint32_t ran;              // out: cases executed (iterations counted once)
    uint32_t passed;           // out
    uint32_t failed;           // out
    uint32_t rc;               // out: 0 = all selected cases passed
    char message[256];         // out: summary or first failure
};

struct cxl_tc_coverage {
    uint32_t total;            // registered testcases
    uint32_t tested;           // passed at least once in this process
    uint32_t untested;         // registered but never run
    uint32_t reserved;
    char text[8192];           // two sections: tested + untested with reasons
};

#define CXL_TC_GET_COUNT  _IOR(CXL_TC_MAGIC, 0x01, uint32_t)
#define CXL_TC_GET_INFO   _IOWR(CXL_TC_MAGIC, 0x02, struct cxl_tc_info)
#define CXL_TC_RUN        _IOWR(CXL_TC_MAGIC, 0x03, struct cxl_tc_run)
#define CXL_TC_RUN_ALL    _IOWR(CXL_TC_MAGIC, 0x04, struct cxl_tc_run)
#define CXL_TC_COVERAGE   _IOR(CXL_TC_MAGIC, 0x05, struct cxl_tc_coverage)
