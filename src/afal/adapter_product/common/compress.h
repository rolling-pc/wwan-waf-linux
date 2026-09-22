#ifndef AFAL_ADAPTER_PRODUCT_COMMON_COMPRESS_H_
#define AFAL_ADAPTER_PRODUCT_COMMON_COMPRESS_H_

#include "common.h"
#include "lzma.h"
#include <mutex>
#include <utility>
#include <memory>
#include <qobject.h>
#include <qthread.h>
#include <qfile.h>
#include <qobjectdefs.h>
namespace afal {

class Compress : public QThread {
    Q_OBJECT
 public:
    Compress() = default;
    Compress(const Compress&) = delete;
    Compress(Compress&&) = delete;
    Compress& operator=(const Compress&) = delete;
    Compress& operator=(Compress&&) = delete;
    ~Compress() override = default;

    // sync 
    virtual bool Run() = 0;

    // async
    virtual void Start() = 0;
    virtual void Stop() = 0;
    
    static std::unique_ptr<Compress> Create(const QString & in_file, const QString & out_file, CompressedFormat format, int level);
    static QString GetSuffix(CompressedFormat format);

 signals:
    void FinishedSignal(bool compress_result);
    void ProgressSignal(int progress);
  protected:
   signals:
    void StartSignal();
}; // class Compress

class Xz : public QObject {
    Q_OBJECT
  public:
    Xz() = delete;
    Xz(const Xz&) = delete;
    Xz(Xz&&) = delete;
    Xz& operator=(const Xz&) = delete;
    Xz& operator=(Xz&&) = delete;
    ~Xz() override = default;
    Xz(const QString& infile, const QString& outfile, int level);

    void Run();
    bool XzInitEncoder();
    bool XzCompressFile();
    void Stop() { stop_flag = true; }
  signals:
    void ProgressSignal(int _t1);
    void FinishedSignal(bool _t1);

  private:
    lzma_stream stream{};
    std::unique_ptr<lzma_stream, decltype(&lzma_end)> auto_strm;
    int level;
    QFile infile;
    QFile outfile;
    std::atomic<bool> stop_flag;

    lzma_mt mtOptions{};

}; // class Compress

class CompressImpl : public Compress {
    Q_OBJECT
  public:
    bool Run() override { return true; }

    void Start() override { FinishedSignal(true); }

    void Stop() override {}

    CompressImpl(const CompressImpl&) = delete;
    CompressImpl(CompressImpl&&) = delete;
    CompressImpl& operator=(const CompressImpl&) = delete;
    CompressImpl& operator=(CompressImpl&&) = delete;
    CompressImpl() = default;
    ~CompressImpl() override = default;

};

class CompressXzImpl : public Compress {
    Q_OBJECT
  public:
    bool Run() override;

    void Start() override;
    bool WaitInit();

    void Stop() override;

    void run() override;

    CompressXzImpl(const CompressXzImpl&) = delete;
    CompressXzImpl(CompressXzImpl&&) = delete;
    CompressXzImpl& operator=(const CompressXzImpl&) = delete;
    CompressXzImpl& operator=(CompressXzImpl&&) = delete;
    CompressXzImpl() = delete;
    ~CompressXzImpl() override;

    CompressXzImpl(QString infile, CompressedFormat format, QString outfile,
                 int level)
        : infile(std::move(infile)), format(format),
          outfile(std::move(outfile)), level(level) {}

  private:
    CompressedFormat format;
    std::mutex init_mtx;
    std::condition_variable cv;
    bool init_flag{};
    int level;
    QString infile;
    QString outfile;

    std::unique_ptr<Xz> xz;
};

}  // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_COMMON_COMPRESS_H_