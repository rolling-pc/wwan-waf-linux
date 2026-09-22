#include "file.h"
#include "log.hpp"
#ifdef _IS_LINUX_
#include <cerrno>
#include <cstddef>
#include <sys/types.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define read(fd, buffer, count) recv(fd, (char*)buffer, count, 0)
#endif

namespace afal {
namespace {

using namespace log;

}  // namespace

ssize_t ReadWithTimeout(int fd, void* buffer, size_t count, // NOLINT
                        int timeoutSeconds, const int* pipe_fd) {
    fd_set readfds;
    timeval timeout{};

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    int max_fd = fd + 1;
    if (pipe_fd) {
        FD_SET(*pipe_fd, &readfds);
        max_fd = std::max(fd, *pipe_fd) + 1;
    }

    timeout.tv_sec = timeoutSeconds;
    timeout.tv_usec = 0;

    timeval *timeout_ptr = nullptr;

    if (timeoutSeconds == -1) {
        timeout_ptr = nullptr;
    } else {
        timeout_ptr = &timeout;
    }

    int ret = select(max_fd, &readfds, nullptr, nullptr, timeout_ptr);

    if (ret < 0)
    {
        FMTLOG_ERROR("select error : {}", strerror(errno));
        return -1;
    }
    if (ret == 0)
    {
        FMTLOG_ERROR("select return 0");
        return -1;
    }

    if (pipe_fd && FD_ISSET(*pipe_fd, &readfds)) {
        FMTLOG_INFO("pipe fd is set");
        return -1;
    }
    
    return read(fd, buffer, count);
}
}  // namespace afal