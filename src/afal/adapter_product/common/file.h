#ifndef AFAL_ADAPTER_PRODUCT_COMMON_FILE_H_
#define AFAL_ADAPTER_PRODUCT_COMMON_FILE_H_

#include "compress.h"
#ifdef _IS_LINUX_
#include <unistd.h>
#else
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

namespace afal {

namespace {
const int K_READ_TIMEOUT = 10;
}

ssize_t ReadWithTimeout(int fd, void* buffer, size_t count, // NOLINT
                        int timeoutSeconds = K_READ_TIMEOUT, const int* pipe_fd = nullptr);
#if 0
class ScopedFd {
  public:
    ScopedFd(const ScopedFd&) = delete;
    ScopedFd(ScopedFd&&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;
    ScopedFd& operator=(int fd) {
        reset(fd);
        return *this;
    }
    ScopedFd& operator=(ScopedFd&&) = delete;
    ScopedFd(int fd) : fd(fd) {}
    ~ScopedFd() { if (fd > 0) close(fd); }
    [[nodiscard]] int get() const { return fd; }

    void reset(int fd = 0) { 
        if (this->fd > 0) {
            close(this->fd);
        }
        this->fd = fd;
    }

  private:
    std::atomic<int> fd;
};
#endif

class ScopedFd {
    public:
        ScopedFd() : fd_(-1) {}
        explicit ScopedFd(int fd) : fd_(fd) {}
    
        ~ScopedFd() { closeFd(); }
    
        ScopedFd(const ScopedFd&) = delete;
        ScopedFd& operator=(const ScopedFd&) = delete;
    
        ScopedFd(ScopedFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
        ScopedFd& operator=(ScopedFd&& other) noexcept {
            if (this != &other) {
                closeFd();
                fd_ = other.fd_;
                other.fd_ = -1;
            }
            return *this;
        }
    
        int get() const { return fd_; }
        int release() { int tmp = fd_; fd_ = -1; return tmp; }
    
        void reset(int newFd = -1) {
            closeFd();
            fd_ = newFd;
        }
    
    private:
        int fd_;
    
        void closeFd() {
            if (fd_ >= 0) {
                // 不关闭标准 fd 0/1/2
                if (fd_ >= 3) close(fd_);
                fd_ = -1;
            }
        }
    };
class ScopedAutoCloseQFile {
  public:
    ScopedAutoCloseQFile(const ScopedAutoCloseQFile&) = delete;
    ScopedAutoCloseQFile(ScopedAutoCloseQFile&&) = delete;
    ScopedAutoCloseQFile& operator=(const ScopedAutoCloseQFile&) = delete;
    ScopedAutoCloseQFile& operator=(ScopedAutoCloseQFile&&) = delete;
    ScopedAutoCloseQFile(QFile* file) : file(file){};
    ~ScopedAutoCloseQFile() {
        file->close();
    }
  private:
    QFile *file{};
};

}

#endif  // AFAL_ADAPTER_PRODUCT_COMMON_FILE_H_