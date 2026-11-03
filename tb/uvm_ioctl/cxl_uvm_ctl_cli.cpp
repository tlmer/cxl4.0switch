// -----------------------------------------------------------------------------
// cxl_uvm_ctl_cli.cpp -- user CLI for the CXL UVM ioctl-style control socket.
//
//   cxl_uvm_ctl list
//   cxl_uvm_ctl run 3
//   cxl_uvm_ctl run all 2
//   cxl_uvm_ctl coverage
//   cxl_uvm_ctl quit
//
//   With no arguments, one command is read from stdin and sent.
// -----------------------------------------------------------------------------
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char** argv) {
    const char* path = "/tmp/cxl_uvm_ctl.sock";
    std::string cmd;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) cmd += ' ';
        cmd += argv[i];
    }
    if (cmd.empty()) {
        std::getline(std::cin, cmd);
    }
    if (cmd.empty()) {
        std::fprintf(stderr, "usage: %s list | run <id|all> [n] | coverage | quit\n", argv[0]);
        return 2;
    }

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { std::perror("socket"); return 1; }
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::perror("connect(/tmp/cxl_uvm_ctl.sock)");
        ::close(fd);
        return 1;
    }
    cmd += '\n';
    ssize_t wn = ::write(fd, cmd.data(), cmd.size());
    if (wn != (ssize_t)cmd.size()) { std::perror("write"); ::close(fd); return 1; }

    char buf[4096];
    for (;;) {
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0) {
            std::fwrite(buf, 1, (size_t)n, stdout);
            continue;
        }
        if (n == 0) break;
        std::perror("read");
        break;
    }
    ::close(fd);
    return 0;
}
