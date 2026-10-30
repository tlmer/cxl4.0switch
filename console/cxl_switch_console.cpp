// -----------------------------------------------------------------------------
// cxl_switch_console.cpp -- text console for the CXL switch model (M4.7).
//
//   Model commands: discover | ls | hdm | region | mbox <port> |
//                   read <port> <off> | write <port> <off> <hex,...> |
//                   event <port> | poison <port> | perf | trace on|off|reset |
//                   cover | run all | quit
//   Testcase channel (independent of the DUT UAPI):
//                   tc list | tc run <id|all> [iterations] | tc cover
//
//   Batch options: --dts <path> --run-all --cover --tc <id|all> [n]
//                  --tc-list --tc-cover
// -----------------------------------------------------------------------------
#include "cxl_dts.h"
#include "cxl_linux.h"
#include "cxl_tc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int run_testcase(const char* what, std::uint32_t iterations) {
    std::uint32_t id = CXL_TC_ID_ALL;
    if (std::strcmp(what, "all") != 0) {
        char* end = nullptr;
        const unsigned long v = std::strtoul(what, &end, 0);
        if (end && *end == '\0') {
            id = (std::uint32_t)v;
        } else {
            bool found = false;
            for (std::uint32_t i = 0; i < cxl_tlm::cxl_tc_get_count(); ++i) {
                cxl_tlm::CxlTcCaseInfo info;
                if (cxl_tlm::cxl_tc_get_info(i, &info) && info.name == what) {
                    id = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::fprintf(stderr, "unknown testcase: %s\n", what);
                return 1;
            }
        }
    }
    ::cxl_tc_run r;
    std::memset(&r, 0, sizeof(r));
    r.id = id;
    r.iterations = iterations;
    std::string log;
    const bool ok = cxl_tlm::cxl_tc_execute(id, iterations, &r, &log);
    std::printf("%s", log.c_str());
    std::printf("[console] tc run => %s (%u passed, %u failed; %u iterations)\n",
                ok ? "PASS" : "FAIL", r.passed, r.failed, r.iterations);
    if (!ok) std::printf("[console] %s\n", r.message);
    return ok ? 0 : 1;
}

int list_testcases() {
    const std::uint32_t n = cxl_tlm::cxl_tc_get_count();
    std::printf("%u testcases\n", n);
    for (std::uint32_t i = 0; i < n; ++i) {
        cxl_tlm::CxlTcCaseInfo info;
        if (cxl_tlm::cxl_tc_get_info(i, &info))
            std::printf("  %2u  %-18s %s\n", i, info.name.c_str(),
                        info.description.c_str());
    }
    return 0;
}

int print_coverage() {
    ::cxl_tc_coverage cov;
    if (!cxl_tlm::cxl_tc_get_coverage(&cov)) return 1;
    std::printf("%s", cov.text);
    return 0;
}

bool parse_u32(const char* s, std::uint32_t* out) {
    char* end = nullptr;
    const unsigned long v = std::strtoul(s, &end, 0);
    if (!end || *end != '\0') return false;
    *out = (std::uint32_t)v;
    return true;
}

int console_tc(const std::string& line) {
    std::istringstream is(line);
    std::string cmd, what;
    is >> cmd >> what;
    if (what.empty() || what == "list") return list_testcases();
    if (what == "cover") return print_coverage();
    if (what == "run") {
        std::string id, iter;
        is >> id >> iter;
        std::uint32_t n = 1;
        if (!iter.empty() && !parse_u32(iter.c_str(), &n)) {
            std::printf("bad iterations\n");
            return 1;
        }
        return run_testcase(id.empty() ? "all" : id.c_str(), n);
    }
    std::printf("usage: tc list | tc run <id|all> [iterations] | tc cover\n");
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    std::string dts_path;
    bool batch = false;
    bool kernel_log = false;
    int batch_rc = 0;

    cxl_tlm::CxlLinuxProxy proxy;
    std::string err;
    // Parse options that affect initialization first.
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--dts") && i + 1 < argc) dts_path = argv[++i];
        else if (!std::strcmp(argv[i], "--kernel-log")) kernel_log = true;
    }

    if (!dts_path.empty()) {
        cxl_tlm::CxlDtsConfig cfg;
        if (!cxl_tlm::cxl_model_load_dts(dts_path, &cfg, &err) ||
            !proxy.init(cfg, &err)) {
            std::fprintf(stderr, "DTS init failed: %s\n", err.c_str());
            return 1;
        }
        std::printf("[console] DTS %s: %d ports, %s\n", dts_path.c_str(),
                    cfg.num_ports, cxl_tlm::cxl_mode_name(cfg.mode));
    } else if (!proxy.init(4, &err)) {
        std::fprintf(stderr, "init failed: %s\n", err.c_str());
        return 1;
    }
    proxy.set_kernel_log(kernel_log);
    proxy.acpi_view();

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--run-all")) {
            batch = true;
            const bool ok = proxy.run_all();
            std::printf("%s\n", proxy.log.c_str());
            std::printf("[console] run all => %s\n", ok ? "PASS" : "FAIL");
            if (!ok) batch_rc = 1;
        } else if (!std::strcmp(argv[i], "--cover")) {
            std::printf("%s", proxy.cover().c_str());
            return 0;
        } else if (!std::strcmp(argv[i], "--tc-list")) {
            return list_testcases();
        } else if (!std::strcmp(argv[i], "--tc-cover")) {
            return print_coverage();
        } else if (!std::strcmp(argv[i], "--tc")) {
            if (i + 1 >= argc) { std::fprintf(stderr, "--tc needs an id or all\n"); return 1; }
            const char* what = argv[++i];
            std::uint32_t n = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                if (!parse_u32(argv[i + 1], &n)) {
                    std::fprintf(stderr, "bad iterations\n");
                    return 1;
                }
                ++i;
            }
            return run_testcase(what, n);
        } else if (!std::strcmp(argv[i], "--dts")) {
            ++i;
        }
    }
    if (batch) return batch_rc;

    std::printf("CXL switch console (type 'help' for commands)\n");
    std::string line;
    while (true) {
        std::printf("cxl> ");
        std::fflush(stdout);
        if (!std::getline(std::cin, line)) break;
        if (line == "help") {
            std::printf("discover ls lspci cxl list hdm region mbox <port> "
                        "read <port> <off> write <port> <off> <hex,...> "
                        "event <port> poison <port> perf trace on|off|reset "
                        "klog on|off|show cover run all "
                        "tc list|run <id|all> [n]|cover quit\n");
            continue;
        }
        if (line == "quit" || line == "exit") break;
        if (line.rfind("tc", 0) == 0 && (line.size() == 2 || line[2] == ' ')) {
            console_tc(line);
            continue;
        }
        std::string out;
        const bool ok = proxy.console(line, &out);
        if (!out.empty()) std::printf("%s", out.c_str());
        if (!ok && out.empty()) std::printf("error\n");
    }
    return 0;
}
