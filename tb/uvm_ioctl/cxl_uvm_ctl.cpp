// -----------------------------------------------------------------------------
// cxl_uvm_ctl.cpp -- Unix-domain control transport for the CXL UVM TB.
//
//   No threads: the UVM TB polls cxl_uvm_ctl_poll() every 1 ms.  The helper
//   accepts one client, parses one line, and returns a command code.  The UVM
//   TB calls the matching cxl_uvm_ctl_*() function, which executes the model
//   testcase runner and writes the response.
// -----------------------------------------------------------------------------
#include "cxl_uvm_ctl.h"

#include "cxl_tc.h"
#include "cxl_tc_ref.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

int  g_listen_fd = -1;
int  g_client_fd = -1;
int  g_cmd_id = -1;
int  g_cmd_iters = 1;
std::string g_path = "/tmp/cxl_uvm_ctl.sock";
std::string g_line;

void close_client() {
    if (g_client_fd >= 0) {
        ::close(g_client_fd);
        g_client_fd = -1;
    }
    g_line.clear();
    g_cmd_id = -1;
    g_cmd_iters = 1;
}

void respond(const std::string& text) {
    if (g_client_fd >= 0) {
        const char* p = text.data();
        size_t left = text.size();
        while (left > 0) {
            ssize_t n = ::write(g_client_fd, p, left);
            if (n <= 0) break;
            p += n;
            left -= (size_t)n;
        }
    }
    close_client();
}

std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

int parse_and_queue(const std::string& raw, std::string* err) {
    const std::string line = trim(raw);
    if (line.empty()) { *err = "empty command"; return CXL_UVM_CTL_NONE; }
    const std::string cmd = lower(line);
    if (cmd == "list") return CXL_UVM_CTL_LIST;
    if (cmd == "coverage") return CXL_UVM_CTL_COVERAGE;
    if (cmd == "quit" || cmd == "exit") return CXL_UVM_CTL_QUIT;
    if (cmd.rfind("run", 0) == 0) {
        std::string rest = trim(line.substr(3));
        if (rest.empty()) { *err = "RUN needs <id|all> [iterations]"; return CXL_UVM_CTL_NONE; }
        size_t sp = rest.find_first_of(" \t");
        std::string idtxt = (sp == std::string::npos) ? rest : rest.substr(0, sp);
        std::string itxt  = (sp == std::string::npos) ? "" : trim(rest.substr(sp + 1));
        if (lower(idtxt) == "all") {
            g_cmd_id = -1;
        } else {
            char* end = nullptr;
            long v = std::strtol(idtxt.c_str(), &end, 0);
            if (!end || *end || v < 0) { *err = "bad testcase id"; return CXL_UVM_CTL_NONE; }
            g_cmd_id = (int)v;
        }
        g_cmd_iters = 1;
        if (!itxt.empty()) {
            char* end = nullptr;
            long v = std::strtol(itxt.c_str(), &end, 0);
            if (!end || *end || v < 1 || v > 10000) { *err = "iterations must be 1..10000"; return CXL_UVM_CTL_NONE; }
            g_cmd_iters = (int)v;
        }
        return CXL_UVM_CTL_RUN;
    }
    *err = "unknown command (LIST / RUN <id|all> [n] / COVERAGE / QUIT)";
    return CXL_UVM_CTL_NONE;
}

} // namespace

extern "C" int cxl_uvm_ctl_init(const char* path) {
    cxl_uvm_ctl_close();
    if (path && *path) g_path = path;
    ::unlink(g_path.c_str());
    g_listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_listen_fd < 0) return -errno;
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (g_path.size() >= sizeof(addr.sun_path)) { cxl_uvm_ctl_close(); return -ENAMETOOLONG; }
    std::strncpy(addr.sun_path, g_path.c_str(), sizeof(addr.sun_path) - 1);
    if (::bind(g_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        const int e = errno; cxl_uvm_ctl_close(); return -e;
    }
    if (::listen(g_listen_fd, 4) < 0) {
        const int e = errno; cxl_uvm_ctl_close(); return -e;
    }
    int fl = ::fcntl(g_listen_fd, F_GETFL, 0);
    if (fl >= 0) (void)::fcntl(g_listen_fd, F_SETFL, fl | O_NONBLOCK);
    std::printf("CXL_UVM_IOCTL: listening on %s\n", g_path.c_str());
    std::fflush(stdout);
    return 0;
}

extern "C" int cxl_uvm_ctl_poll(int* id, int* iterations) {
    if (id) *id = -1;
    if (iterations) *iterations = 1;
    if (g_listen_fd < 0) return CXL_UVM_CTL_NONE;

    if (g_client_fd < 0) {
        g_client_fd = ::accept(g_listen_fd, nullptr, nullptr);
        if (g_client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return CXL_UVM_CTL_NONE;
            return CXL_UVM_CTL_NONE;
        }
        int fl = ::fcntl(g_client_fd, F_GETFL, 0);
        if (fl >= 0) (void)::fcntl(g_client_fd, F_SETFL, fl | O_NONBLOCK);
        g_line.clear();
    }

    char buf[512];
    for (;;) {
        ssize_t n = ::read(g_client_fd, buf, sizeof(buf));
        if (n > 0) {
            g_line.append(buf, (size_t)n);
            continue;
        }
        if (n == 0) { close_client(); return CXL_UVM_CTL_NONE; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        close_client();
        return CXL_UVM_CTL_NONE;
    }

    size_t nl = g_line.find('\n');
    if (nl == std::string::npos) {
        if (g_line.size() > 1024) { respond("ERROR: command too long\n"); }
        return CXL_UVM_CTL_NONE;
    }
    std::string line = g_line.substr(0, nl);
    // Keep any bytes after the newline for the next poll.
    g_line.erase(0, nl + 1);

    std::string err;
    int cmd = parse_and_queue(line, &err);
    if (cmd == CXL_UVM_CTL_NONE) {
        respond(std::string("ERROR: ") + err + "\n");
        return CXL_UVM_CTL_NONE;
    }
    if (id) *id = g_cmd_id;
    if (iterations) *iterations = g_cmd_iters;
    return cmd;
}

extern "C" void cxl_uvm_ctl_list(void) {
    std::string out;
    const unsigned n = cxl_tlm::cxl_tc_get_count();
    for (unsigned i = 0; i < n; ++i) {
        cxl_tlm::CxlTcCaseInfo info;
        std::string err;
        if (!cxl_tlm::cxl_tc_get_info(i, &info, &err)) {
            out += "ERROR: " + err + "\n";
            continue;
        }
        char line[320];
        std::snprintf(line, sizeof(line), "%u\t%s\t%s\n", i, info.name.c_str(),
                      info.description.c_str());
        out += line;
    }
    respond(out);
}

extern "C" void cxl_uvm_ctl_run(int id, int iterations) {
    cxl_tc_run r;
    std::memset(&r, 0, sizeof(r));
    r.id = (id < 0) ? CXL_TC_ID_ALL : (uint32_t)id;
    r.iterations = (iterations > 0) ? (uint32_t)iterations : 1u;
    std::string log;
    const bool ok = cxl_tlm::cxl_tc_execute(r.id, r.iterations, &r, &log);
    std::printf("CXL_UVM_IOCTL: RUN id=%u iterations=%u => %s\n%s",
                (unsigned)r.id, (unsigned)r.iterations, ok ? "PASS" : "FAIL",
                log.c_str());
    std::fflush(stdout);
    char line[512];
    std::snprintf(line, sizeof(line),
                  "RUN id=%u iterations=%u ok=%d ran=%u passed=%u failed=%u rc=%u\n%s\n",
                  (unsigned)r.id, (unsigned)r.iterations, ok ? 1 : 0,
                  (unsigned)r.ran, (unsigned)r.passed, (unsigned)r.failed,
                  (unsigned)r.rc, r.message);
    respond(line);
}

extern "C" void cxl_uvm_ctl_coverage(void) {
    cxl_tc_coverage cov;
    std::memset(&cov, 0, sizeof(cov));
    std::string log;
    const bool ok = cxl_tlm::cxl_tc_get_coverage(&cov);
    if (!ok) {
        respond("ERROR: coverage unavailable\n");
        return;
    }
    std::printf("CXL_UVM_IOCTL: COVERAGE total=%u tested=%u untested=%u\n",
                (unsigned)cov.total, (unsigned)cov.tested, (unsigned)cov.untested);
    std::printf("%s\n", cov.text);
    std::fflush(stdout);
    char line[256];
    std::snprintf(line, sizeof(line),
                  "COVERAGE total=%u tested=%u untested=%u\n%s\n",
                  (unsigned)cov.total, (unsigned)cov.tested, (unsigned)cov.untested,
                  cov.text);
    respond(line);
}

extern "C" void cxl_uvm_ctl_quit(void) {
    respond("BYE\n");
}

extern "C" void cxl_uvm_ctl_close(void) {
    close_client();
    if (g_listen_fd >= 0) {
        ::close(g_listen_fd);
        g_listen_fd = -1;
    }
    if (!g_path.empty()) (void)::unlink(g_path.c_str());
}
