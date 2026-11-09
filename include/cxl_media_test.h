// -----------------------------------------------------------------------------
// cxl_media_test.h -- Media Test Capability / Results logs (CXL 4.0 8.2.10.5.2.5
// and 8.2.10.5.2.6, Tables 8-258..8-267).
//
//   Optional model profile.  The capability log is a 16-byte common header
//   plus one 16-byte entry per test.  The result logs are a 16-byte common
//   header plus one entry per executed test; the short entry is 0x20 bytes,
//   the long entry is 0x30 bytes plus 0x50 bytes per error signature.
//
//   The actual media test execution is an owner/device action.  This header
//   owns the deterministic log serialization and result bookkeeping; a
//   platform calls add_short_result()/add_long_result() when a test completes.
//   Results are cleared by a new test start or Clear Log.
//
//   Log UUIDs (Table 8-250):
//     e6dfa32c-d13e-4a5c-8ca8-99bebbf731a4  Media Test Capability Log
//     2c255522-8ce4-11ec-b909-0242ac120002  Media Test Results Short Log
//     c1fe0b3e-7a00-448e-a24e-a6aabbfe587a  Media Test Results Long Log
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cxl_tlm {

constexpr std::uint8_t kMediaTestCapabilityUuid[16] = {
    0xe6, 0xdf, 0xa3, 0x2c, 0xd1, 0x3e, 0x4a, 0x5c,
    0x8c, 0xa8, 0x99, 0xbe, 0xbb, 0xf7, 0x31, 0xa4,
};
constexpr std::uint8_t kMediaTestShortUuid[16] = {
    0x2c, 0x25, 0x55, 0x22, 0x8c, 0xe4, 0x11, 0xec,
    0xb9, 0x09, 0x02, 0x42, 0xac, 0x12, 0x00, 0x02,
};
constexpr std::uint8_t kMediaTestLongUuid[16] = {
    0xc1, 0xfe, 0x0b, 0x3e, 0x7a, 0x00, 0x44, 0x8e,
    0xa2, 0x4e, 0xa6, 0xaa, 0xbb, 0xfe, 0x58, 0x7a,
};

struct CxlMediaTestCapability {
    std::uint16_t test_id = 0;
    std::uint8_t algorithm = 0;       // 1=Write/Read/Compare, 2=Checkerboard,
                                      // 3=MARCH, 4=MATS, 5=MATS+, 6=Walking 1s,
                                      // 7=Walking 0s
    std::uint8_t execution_time = 0;  // bits[3:0] scale, bits[7:4] latency
    std::uint16_t capabilities = 0;
    std::uint16_t supported_patterns = 0;
    std::uint8_t prbs_length = 0;     // 0 means PRBS not supported
};

struct CxlMediaTestShortResult {
    std::uint16_t test_id = 0;
    std::uint64_t start_ns = 0;
    std::uint64_t end_ns = 0;
    std::uint8_t result = 0;          // 0 success, 1 failure, 2 aborted
    std::uint8_t flags = 0;           // bit0 error signature list overflow
    std::uint32_t uncorrectable_errors = 0;
    std::uint32_t correctable_errors = 0;
};

struct CxlMediaTestLongResult : CxlMediaTestShortResult {
    std::uint64_t capacity_tested_256m = 0;
    // Raw Table 8-267 error signatures (0x50 bytes each).  The model preserves
    // caller-provided signatures without interpreting the memory location.
    std::vector<std::array<std::uint8_t, 0x50>> signatures;
};

struct CxlMediaTestRequest {
    std::uint8_t num_tests = 0;
    std::uint64_t start_dpa = 0;
    std::uint64_t length = 0;
    std::uint8_t results_config = 0;
    std::uint8_t config_flags = 0;
    std::vector<std::uint16_t> test_ids;
};

struct CxlMediaTestState {
    bool supported = false;
    bool running = false;
    CxlMediaTestRequest request;
    std::uint32_t total_error_signatures = 16;
    std::uint8_t long_version = 1;
    std::uint8_t short_version = 1;
    std::uint8_t capabilities = 0x0F;  // all ECC-disable / metadata test bits
    std::uint8_t short_result_code = 0;
    std::uint8_t long_result_code = 0;
    std::vector<CxlMediaTestCapability> tests;
    std::vector<CxlMediaTestShortResult> short_results;
    std::vector<CxlMediaTestLongResult> long_results;

    void add_default_test()
    {
        CxlMediaTestCapability t;
        t.test_id = 1;
        t.algorithm = 1;               // Write, Read and Compare pattern
        t.execution_time = 0x11;       // 1 x 10 ms scale
        t.capabilities = 0x0007;       // address configurable, inverse pattern,
                                       // exit on uncorrectable
        t.supported_patterns = 0x0007; // user, vendor, PRBS
        t.prbs_length = 7;
        tests.push_back(t);
    }

    void add_short_result(const CxlMediaTestShortResult& r)
    {
        short_results.push_back(r);
        if (r.result != 0) short_result_code = 1;
    }

    void add_long_result(const CxlMediaTestLongResult& r)
    {
        CxlMediaTestLongResult copy = r;
        // The capability log advertises the maximum number of error
        // signatures.  Overflow is reported in the result flags (Table 8-266
        // bit[0]); keep only the advertised capacity.
        if (copy.signatures.size() > total_error_signatures) {
            copy.signatures.resize(total_error_signatures);
            copy.flags |= 0x01;
        }
        long_results.push_back(copy);
        if (copy.result != 0) long_result_code = 1;
    }

    void clear_short_results()
    {
        short_results.clear();
        short_result_code = 0;
    }

    void clear_long_results()
    {
        long_results.clear();
        long_result_code = 0;
    }

    std::vector<std::uint8_t> build_capability_log() const
    {
        std::vector<std::uint8_t> v(0x10 + tests.size() * 0x10, 0);
        v[0x00] = (std::uint8_t)tests.size();
        put32(v, 0x01, total_error_signatures);
        v[0x05] = long_version;
        v[0x06] = short_version;
        v[0x07] = capabilities;
        for (std::size_t i = 0; i < tests.size(); ++i) {
            const auto& t = tests[i];
            const std::size_t o = 0x10 + i * 0x10;
            put16(v, o + 0x00, t.test_id);
            v[o + 0x02] = t.algorithm;
            v[o + 0x03] = t.execution_time;
            put16(v, o + 0x04, t.capabilities);
            put16(v, o + 0x06, t.supported_patterns);
            v[o + 0x08] = t.prbs_length;
        }
        return v;
    }

    std::vector<std::uint8_t> build_short_log() const
    {
        std::vector<std::uint8_t> v(0x10 + short_results.size() * 0x20, 0);
        v[0x00] = (std::uint8_t)short_results.size();
        v[0x01] = short_version;
        v[0x02] = short_result_code;
        for (std::size_t i = 0; i < short_results.size(); ++i) {
            const auto& r = short_results[i];
            const std::size_t o = 0x10 + i * 0x20;
            serialise_short(v, o, r);
            put32(v, o + 0x1C, 0);
        }
        return v;
    }

    std::vector<std::uint8_t> build_long_log() const
    {
        std::size_t n = 0;
        for (const auto& r : long_results)
            n += 0x30 + r.signatures.size() * 0x50;
        std::vector<std::uint8_t> v(0x10 + n, 0);
        v[0x00] = (std::uint8_t)long_results.size();
        v[0x01] = long_version;
        v[0x02] = long_result_code;
        std::size_t o = 0x10;
        for (const auto& r : long_results) {
            serialise_short(v, o, r);
            put64(v, o + 0x20, r.capacity_tested_256m);
            put32(v, o + 0x28, (std::uint32_t)r.signatures.size());
            put32(v, o + 0x2C, 0);
            std::size_t so = o + 0x30;
            for (const auto& sig : r.signatures) {
                std::memcpy(v.data() + so, sig.data(), sig.size());
                so += sig.size();
            }
            o = so;
        }
        return v;
    }

private:
    static void serialise_short(std::vector<std::uint8_t>& v, std::size_t o,
                                const CxlMediaTestShortResult& r)
    {
        put16(v, o + 0x00, r.test_id);
        put64(v, o + 0x02, r.start_ns);
        put64(v, o + 0x0A, r.end_ns);
        v[o + 0x12] = r.result;
        v[o + 0x13] = r.flags;
        put32(v, o + 0x14, r.uncorrectable_errors);
        put32(v, o + 0x18, r.correctable_errors);
    }
    static void put16(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint16_t x)
    {
        v[o] = (std::uint8_t)(x & 0xFF);
        v[o + 1] = (std::uint8_t)(x >> 8);
    }
    static void put32(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint32_t x)
    {
        for (int i = 0; i < 4; ++i) v[o + (std::size_t)i] =
            (std::uint8_t)(x >> (8 * i));
    }
    static void put64(std::vector<std::uint8_t>& v, std::size_t o,
                      std::uint64_t x)
    {
        for (int i = 0; i < 8; ++i) v[o + (std::size_t)i] =
            (std::uint8_t)(x >> (8 * i));
    }
};

} // namespace cxl_tlm
