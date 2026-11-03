// -----------------------------------------------------------------------------
// cxl_axi_dpi_link.cpp -- DPI backend for the model's 128-bit AXI4 master.
//
//   The C++ model thread calls CxlAxiMaster::read/write.  Those calls enqueue a
//   burst in DpiAxiBackend and block on a condition variable.  The simulator
//   thread runs cxl_axi_dpi_master.sv, which polls the queue, drives the AXI4
//   pins, and calls cxl_axi_model_done() when B/R completes.
//
//   This file also contains the self-test started by cxl_axi_model_start().
//   The SV testbench waits for cxl_axi_model_finished() and then checks the
//   SRAM contents through the RTL hierarchy.
// -----------------------------------------------------------------------------
#include "cxl_axi_dpi_link.h"

#include "cxl_axi_master.h"
#include "cxl_cache.h"
#include "cxl_root.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace cxl_tlm;

namespace {

struct DpiReq {
    std::uint64_t handle = 0;
    CxlAxiTxn txn;
    bool started = false;
    bool completed = false;
    CxlAxiResult result;
};

class DpiAxiBackend : public CxlAxiBackend {
public:
    static DpiAxiBackend* g;

    DpiAxiBackend() { g = this; }
    ~DpiAxiBackend() override {
        if (g == this) g = nullptr;
    }

    const char* name() const override { return "cxl_axi_dpi"; }

    void reset() override {
        std::lock_guard<std::mutex> lk(mu_);
        reqs_.clear();
        order_.clear();
        active_ = 0;
        next_handle_ = 1;
        wr_bursts = rd_bursts = 0;
    }

    CxlAxiAccept submit(const CxlAxiTxn& txn, std::uint64_t* handle,
                        std::string* err) override {
        if (handle == nullptr) {
            if (err) *err = "DPI AXI submit needs a handle";
            return CxlAxiAccept::ERROR;
        }
        std::lock_guard<std::mutex> lk(mu_);
        if (reqs_.size() >= (std::size_t)kCxlAxiOutstanding)
            return CxlAxiAccept::RETRY;
        std::shared_ptr<DpiReq> r(new DpiReq());
        r->handle = next_handle_++;
        if (next_handle_ == 0) next_handle_ = 1;
        r->txn = txn;
        reqs_[r->handle] = r;
        order_.push_back(r->handle);
        cv_.notify_all();
        *handle = r->handle;
        return CxlAxiAccept::ACCEPTED;
    }

    CxlAxiPoll poll(std::uint64_t handle, CxlAxiResult* result,
                    std::string* err) override {
        std::unique_lock<std::mutex> lk(mu_);
        auto it = reqs_.find(handle);
        if (it == reqs_.end()) {
            if (err) *err = "DPI AXI completion for an unknown handle";
            return CxlAxiPoll::ERROR;
        }
        const bool ok = cv_.wait_for(
            lk, std::chrono::seconds(120),
            [&] { return !it->second || it->second->completed; });
        if (!ok) {
            if (err) *err = "DPI AXI completion timed out";
            return CxlAxiPoll::ERROR;
        }
        it = reqs_.find(handle);
        if (it == reqs_.end()) {
            if (err) *err = "DPI AXI request vanished";
            return CxlAxiPoll::ERROR;
        }
        std::shared_ptr<DpiReq> r = it->second;
        if (result) *result = r->result;
        reqs_.erase(it);
        for (auto q = order_.begin(); q != order_.end(); ++q) {
            if (*q == handle) { order_.erase(q); break; }
        }
        if (r->txn.write) ++wr_bursts; else ++rd_bursts;
        return CxlAxiPoll::DONE;
    }

    // ---- simulator-thread service -----------------------------------------
    void service_poll(int* valid, unsigned long long* addr, int* size,
                      int* beats, int* id, int* is_write,
                      unsigned long long* handle) {
        std::lock_guard<std::mutex> lk(mu_);
        *valid = 0; *addr = 0; *size = 0; *beats = 0; *id = 0;
        *is_write = 0; *handle = 0;
        if (active_ != 0) return;
        for (std::uint64_t h : order_) {
            auto it = reqs_.find(h);
            if (it == reqs_.end() || it->second->started ||
                it->second->completed) continue;
            std::shared_ptr<DpiReq> r = it->second;
            r->started = true;
            active_ = h;
            *valid = 1;
            *addr = r->txn.addr;
            *size = (int)r->txn.size;
            *beats = (int)r->txn.beats;
            *id = (int)r->txn.id;
            *is_write = r->txn.write ? 1 : 0;
            *handle = h;
            return;
        }
    }

    void service_wdata(unsigned long long handle, int beat,
                       unsigned long long* data_lo, unsigned long long* data_hi,
                       unsigned long long* strb_lo, unsigned long long* strb_hi) {
        std::lock_guard<std::mutex> lk(mu_);
        *data_lo = *data_hi = *strb_lo = *strb_hi = 0;
        auto it = reqs_.find(handle);
        if (it == reqs_.end()) return;
        const DpiReq& r = *it->second;
        if (!r.txn.write || beat < 0 || (unsigned)beat >= r.txn.beats) return;
        const std::size_t off = (std::size_t)beat * kCxlAxiDataBytes;
        std::memcpy(data_lo, r.txn.wdata.data() + off, 8);
        std::memcpy(data_hi, r.txn.wdata.data() + off + 8, 8);
        // The RTL side only needs one bit per byte lane.
        for (int i = 0; i < 8; ++i) {
            if (r.txn.wstrb[off + (std::size_t)i])
                *strb_lo |= (1ull << i);
            if (r.txn.wstrb[off + 8 + (std::size_t)i])
                *strb_hi |= (1ull << i);
        }
    }

    void service_rdata(unsigned long long handle, int beat,
                       unsigned long long data_lo, unsigned long long data_hi) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = reqs_.find(handle);
        if (it == reqs_.end()) return;
        DpiReq& r = *it->second;
        if (r.txn.write || beat < 0 || (unsigned)beat >= r.txn.beats) return;
        const std::size_t off = (std::size_t)beat * kCxlAxiDataBytes;
        if (r.txn.rdata.size() < off + kCxlAxiDataBytes)
            r.txn.rdata.resize(off + kCxlAxiDataBytes, 0);
        std::memcpy(r.txn.rdata.data() + off, &data_lo, 8);
        std::memcpy(r.txn.rdata.data() + off + 8, &data_hi, 8);
    }

    void service_done(unsigned long long handle, int resp) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = reqs_.find(handle);
        if (it == reqs_.end()) return;
        DpiReq& r = *it->second;
        r.result.resp = (resp == 0) ? CxlAxiResp::OKAY : CxlAxiResp::DECERR;
        r.result.beats = r.txn.beats;
        r.result.rdata = r.txn.rdata;
        r.completed = true;
        active_ = 0;
        cv_.notify_all();
    }

    std::uint64_t wr_bursts = 0, rd_bursts = 0;

private:
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::map<std::uint64_t, std::shared_ptr<DpiReq>> reqs_;
    std::deque<std::uint64_t> order_;
    std::uint64_t active_ = 0;
    std::uint64_t next_handle_ = 1;
};

DpiAxiBackend* DpiAxiBackend::g = nullptr;

std::atomic<int> g_finished(0);
std::atomic<int> g_failures(0);
std::atomic<int> g_started(0);
std::thread g_thread;

int test_pass = 0, test_fail = 0;
void check(bool ok, const char* what) {
    if (ok) { ++test_pass; std::printf("  [PASS] %s\n", what); }
    else    { ++test_fail; std::printf("  [FAIL] %s\n", what); }
}
void fill(std::uint8_t* p, std::size_t n, unsigned seed) {
    for (std::size_t i = 0; i < n; ++i) p[i] = (std::uint8_t)(seed + i * 7);
}

void run_self_test() {
    std::string e;
    DpiAxiBackend be;
    CxlAxiMaster axi;
    check(axi.init(&be, &e), "D0 DPI AXI backend attaches to the model master");

    {
        std::uint8_t w[64], r[64];
        fill(w, 64, 0x11);
        std::memset(r, 0, sizeof(r));
        check(axi.write(0x1000, w, 64, &e), "D1 64B model write crosses the AXI4 pins");
        check(axi.read(0x1000, r, 64, &e) && std::memcmp(w, r, 64) == 0,
              "D2 64B model read returns SRAM data");
    }
    {
        std::vector<std::uint8_t> w(4096), r(4096);
        fill(w.data(), w.size(), 0x01);
        check(axi.write(0x2000, w.data(), w.size(), &e),
              "D3 4 KiB model write = 256 x 128-bit AXI beats");
        check(axi.read(0x2000, r.data(), r.size(), &e) && r == w,
              "D4 4 KiB model read matches every byte");
    }
    {
        std::uint8_t base[16], w[5], r[5];
        fill(base, 16, 0x70);
        fill(w, 5, 0x30);
        check(axi.write(0x3000, base, 16, &e) &&
              axi.write(0x3003, w, 5, &e) &&
              axi.read(0x3003, r, 5, &e) && std::memcmp(w, r, 5) == 0,
              "D5 unaligned 5B write/read uses byte strobes");
    }
    {
        CxlRoot root;
        root.init(4, 1ull << 20, 0);
        root.attach_axi(&axi);
        std::uint8_t w[64], r[64];
        fill(w, 64, 0xC3);
        check(root.mem_write(0x5000, w, 64, &e) &&
              root.mem_read(0x5000, r, 64, &e) &&
              std::memcmp(w, r, 64) == 0 && root.axi_writes == 1 &&
              root.axi_reads == 1,
              "D6 CxlRoot host memory is backed by the external SRAM");
    }
    {
        CxlCacheSystem cs;
        check(cs.init(CxlFlitMode::B68, &e), "D7 CXL.cache Type2 init");
        cs.attach_axi(&axi);
        const std::uint64_t a = 0x4000;
        std::uint8_t line[64], w[64], got[64];
        fill(line, 64, 0x22);
        cs.home.preload(a, line);                 // EP-side home preload -> AXI write
        check(cs.cpu_read(a, got, &e) && std::memcmp(got, line, 64) == 0,
              "D8 EP read miss fetches the line from SRAM (AXI read)");
        fill(w, 64, 0x44);
        check(cs.cpu_write(a, w, &e) && cs.cpu_evict(a, &e),
              "D9 EP dirty line evicts over AXI");
        check(cs.home.read_mem(a, got) && std::memcmp(got, w, 64) == 0,
              "D10 EP dirty data is visible in external SRAM");
    }
    {
        std::uint8_t w[64], r[64], wrong[64];
        fill(w, 64, 0x66);
        std::memcpy(wrong, w, 64);
        wrong[17] ^= 0x80;
        check(axi.write(0x6000, w, 64, &e) &&
              axi.read(0x6000, r, 64, &e) &&
              std::memcmp(w, r, 64) == 0 && std::memcmp(wrong, r, 64) != 0,
              "R1 wrong data expectation is detectable");
    }

    std::printf("TB_CXL_AXI_DPI: %s (%d/%d)\n",
                test_fail == 0 ? "ALL PASS" : "FAIL",
                test_pass, test_pass + test_fail);
    std::fflush(stdout);
    g_failures = test_fail;
    g_finished = 1;
}

} // namespace

extern "C" void cxl_axi_model_start(void) {
    if (g_started.exchange(1)) return;
    g_thread = std::thread([] {
        run_self_test();
    });
    g_thread.detach();
}

extern "C" int cxl_axi_model_finished(void) {
    return g_finished.load();
}

extern "C" int cxl_axi_model_failures(void) {
    return g_failures.load();
}

extern "C" void cxl_axi_model_poll(int* valid, unsigned long long* addr,
                                   int* size, int* beats, int* id,
                                   int* is_write, unsigned long long* handle) {
    if (!DpiAxiBackend::g) {
        if (valid) *valid = 0;
        return;
    }
    DpiAxiBackend::g->service_poll(valid, addr, size, beats, id, is_write, handle);
}

extern "C" void cxl_axi_model_wdata(unsigned long long handle, int beat,
                                    unsigned long long* data_lo,
                                    unsigned long long* data_hi,
                                    unsigned long long* strb_lo,
                                    unsigned long long* strb_hi) {
    if (!DpiAxiBackend::g) return;
    DpiAxiBackend::g->service_wdata(handle, beat, data_lo, data_hi, strb_lo, strb_hi);
}

extern "C" void cxl_axi_model_rdata(unsigned long long handle, int beat,
                                    unsigned long long data_lo,
                                    unsigned long long data_hi) {
    if (!DpiAxiBackend::g) return;
    DpiAxiBackend::g->service_rdata(handle, beat, data_lo, data_hi);
}

extern "C" void cxl_axi_model_done(unsigned long long handle, int resp) {
    if (!DpiAxiBackend::g) return;
    DpiAxiBackend::g->service_done(handle, resp);
}
