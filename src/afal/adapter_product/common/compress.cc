#include "compress.h"
#include "common.h"
#include "proccommon.h"
#include "log.hpp"
#include "lzma.h"
#include <memory>
#include <mutex>
#include <semaphore>
#include <utility>
#include <qobject.h>
#include <qobjectdefs.h>
#include <thread>

namespace afal {

namespace {

void FakeDeleter(lzma_stream* strm) noexcept {}

using namespace log;
const int kXzTimeout = 300;

} // namespace

Xz::Xz(const QString& infile, const QString& outfile, int level)
    : stop_flag(false), infile(infile), outfile(outfile), level(level),
      auto_strm(nullptr, &FakeDeleter) {
    mtOptions.threads =
        std::max(1, int(std::thread::hardware_concurrency()) - 1);
    FMTLOG_DEBUG(
        "Compress thread created, input file {}, output file {}, threads: {}",
        infile, outfile, mtOptions.threads);
    mtOptions.block_size = 0;
    mtOptions.timeout = kXzTimeout;
    mtOptions.preset = level;
}

CompressXzImpl::~CompressXzImpl() {
    FMTLOG_DEBUG("~CompressImpl called");
    quit();
    wait();
    FMTLOG_DEBUG("~CompressImpl end");
}

bool CompressXzImpl::Run() {
    Start();
    wait();
    return true;
}

void CompressXzImpl::Start() {
    FMTLOG_DEBUG("Start");
    emit StartSignal();
}

bool CompressXzImpl::WaitInit() {
    std::unique_lock<std::mutex> lock(init_mtx);
    if (init_flag)
    {
        return true;
    }

    if (cv.wait_for(lock, std::chrono::seconds(3),
                    [this] { return init_flag; }))
    {
        return true;
    }
    FMTLOG_WARN("Init timeout");
    return false;
}

void CompressXzImpl::Stop() {
    xz->Stop();
}

void CompressXzImpl::run() {
    LOG_INFO("Compress thread started");

    xz = std::make_unique<Xz>(infile, outfile, level);
    connect(this, &CompressXzImpl::StartSignal, xz.get(), &Xz::Run);
    connect(xz.get(), &Xz::FinishedSignal, xz.get(), [this](bool ret) {
        FMTLOG_DEBUG("Compress finished");
        emit this->FinishedSignal(ret);
        quit();
    });
    connect(xz.get(), &Xz::ProgressSignal, xz.get(),
            [this](int progress) { emit this->ProgressSignal(progress); });

    {
        std::lock_guard<std::mutex> lock(init_mtx);
        init_flag = true;
        cv.notify_all();
    }
    exec();
    xz.reset();
}

bool Xz::XzInitEncoder() {
    FMTLOG_INFO("Xz Init encoder.");
    if (!infile.open(QIODevice::ReadOnly) ||
        !outfile.open(QIODevice::WriteOnly))
    {
        FMTLOG_ERROR("File open failed!");
        return false;
    }
    stream = LZMA_STREAM_INIT;
    auto_strm =
        std::unique_ptr<lzma_stream, decltype(&lzma_end)>(&stream, &lzma_end);

    lzma_ret ret = lzma_stream_encoder_mt(
        &stream, &mtOptions); //  level, LZMA_CHECK_CRC64);

    if (ret == LZMA_OK) return true;

    const char* msg = nullptr;
    switch (ret)
    {
        case LZMA_MEM_ERROR:
            msg = "Memory allocation failed";
            break;

        case LZMA_OPTIONS_ERROR:
            msg = "Specified preset is not supported";
            break;

        case LZMA_UNSUPPORTED_CHECK:
            msg = "Specified integrity check is not supported";
            break;

        default:
            msg = "Unknown error, possibly a bug";
            break;
    }

    FMTLOG_ERROR("Error initializing the encoder: {} (error code {})\n", msg,
                 int(ret));
    return false;
}

void Xz::Run() {
    auto ret = XzInitEncoder();
    if (ret) ret = XzCompressFile();
    emit FinishedSignal(ret);
}

bool Xz::XzCompressFile() {
    lzma_action action = LZMA_RUN;

    const size_t in_buffer_size = BUFSIZ;
    const size_t out_buffer_size = BUFSIZ;
    std::vector<uint8_t> in_buffer(in_buffer_size);
    std::vector<uint8_t> out_buffer(out_buffer_size);

    auto_strm->next_in = nullptr;
    auto_strm->avail_in = 0;
    auto_strm->next_out = out_buffer.data();
    auto_strm->avail_out = out_buffer.size();
    FMTLOG_DEBUG("Xz compress file");
    emit ProgressSignal(0);
    qint64 size = infile.size();
    qint64 compress_size = 0;
    int last_progress = 0;

    while (true)
    {
        if (auto_strm->avail_in == 0 && !infile.atEnd())
        {
            auto_strm->next_in = in_buffer.data();
            auto_strm->avail_in =
                infile.read(reinterpret_cast<char*>(in_buffer.data()),
                            int(in_buffer.size()));

            if (auto_strm->avail_in == -1)
            {
                FMTLOG_ERROR("File {} read error occurred", infile.fileName());
                return false;
            }

            compress_size += (qint64)auto_strm->avail_in;
            int progress = int(compress_size * 100 / size); // NOLINT
            if (progress != last_progress)
            {
                last_progress = progress;
                emit ProgressSignal(last_progress);
            }

            if (infile.atEnd())
            {
                action = LZMA_FINISH;
            }
        }

        lzma_ret ret = lzma_code(auto_strm.get(), action);

        if (auto_strm->avail_out == 0 || ret == LZMA_STREAM_END)
        {
            size_t write_size = out_buffer.size() - auto_strm->avail_out;
            size_t bytes_written = outfile.write(
                reinterpret_cast<char*>(out_buffer.data()), int(write_size));
            if (ret == LZMA_STREAM_END)
            {
                FMTLOG_DEBUG("Write {} bytes to file {}, actual writing {}",
                             write_size, outfile.fileName(), bytes_written);
            }
            if (bytes_written != write_size)
            {
                FMTLOG_ERROR("Write error: {}", strerror(errno));
                return false;
            }

            auto_strm->next_out = out_buffer.data();
            auto_strm->avail_out = out_buffer.size();
        }

        if (ret != LZMA_OK)
        {
            if (ret == LZMA_STREAM_END)
            {
                FMTLOG_DEBUG("Compression finished");
                return true;
            }

            const char* msg = nullptr;
            switch (ret)
            {
                case LZMA_MEM_ERROR:
                    msg = "Memory allocation failed";
                    break;

                case LZMA_DATA_ERROR:
                    msg = "File size limits exceeded";
                    break;

                default:
                    msg = "Unknown error, possibly a bug";
                    break;
            }

            FMTLOG_ERROR("Encoder error: {} (error code {})", msg, int(ret));
            return false;
        }

        if (stop_flag)
        {
            FMTLOG_INFO(
                "Compress thread stopped by user, remove output file {}",
                outfile.fileName());
            outfile.close();
            outfile.remove();
            return false;
        }
    }
}

std::unique_ptr<Compress> Compress::Create(const QString& in_file,
                                           const QString& out_file,
                                           CompressedFormat format, int level) {
    if (format == CompressedFormat::NONE) {
        return std::make_unique<CompressImpl>();
    }
    auto compress =
        std::make_unique<CompressXzImpl>(in_file, format, out_file, level);
    compress->start();
    if (!compress->WaitInit())
    {
        FMTLOG_ERROR("Create compress failed!");
        return nullptr;
    }
    return compress;
}

QString Compress::GetSuffix(CompressedFormat format) {
    if (format == CompressedFormat::XZ)
    {
        return ".xz";
    }
    if (format == CompressedFormat::ZIP)
    {
        return ".zip";
    }
    return "";
}

} // namespace afal
