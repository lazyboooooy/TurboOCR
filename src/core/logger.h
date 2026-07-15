#pragma once
#include <QString>
#include <QFile>
#include <QMutex>
#include <QDateTime>

/// 轻量级日志系统 — 同时输出到控制台(stderr)和文件
///
/// 用法:
///   Logger::instance().init("app.log");
///   TLOG_INFO() << "引擎初始化完成, 线程数=" << n;
///   TLOG_WARN() << "检测到低置信度: " << score;
///   TLOG_ERROR() << "初始化失败: " << err;
///
class Logger {
public:
    enum Level { DEBUG_L = 0, INFO_L = 1, WARN_L = 2, ERROR_L = 3 };

    static Logger& instance();

    /// 初始化日志文件（应在 main 中调用一次）
    void init(const QString& logPath);
    bool isInited() const { return inited_; }

    /// 设置最低输出级别（默认 DEBUG）
    void setLevel(Level lv) { level_ = lv; }
    Level level() const { return level_; }

    /// 写入日志（通常通过宏调用）
    void write(Level lv, const char* file, int line, const char* func, const QString& msg);

private:
    Logger() = default;
    QFile    file_;
    QMutex   mutex_;
    Level    level_ = DEBUG_L;
    bool     inited_ = false;

    static const char* levelStr(Level lv);
};

/// RAII 流式写入 helper — 析构时自动 flush
class LogStream {
public:
    LogStream(Logger::Level lv, const char* file, int line, const char* func)
        : lv_(lv), file_(file), line_(line), func_(func) {}

    ~LogStream();

    LogStream& operator<<(const char* v)      { ss_ += QString::fromUtf8(v); return *this; }
    LogStream& operator<<(const QString& v)   { ss_ += v; return *this; }
    LogStream& operator<<(const std::string& v){ ss_ += QString::fromStdString(v); return *this; }
    LogStream& operator<<(int v)              { ss_ += QString::number(v); return *this; }
    LogStream& operator<<(unsigned int v)     { ss_ += QString::number(v); return *this; }
    LogStream& operator<<(long v)             { ss_ += QString::number(v); return *this; }
    LogStream& operator<<(unsigned long v)    { ss_ += QString::number(v); return *this; }
    LogStream& operator<<(long long v)        { ss_ += QString::number(v); return *this; }
    LogStream& operator<<(unsigned long long v){ ss_ += QString::number(v); return *this; }
    LogStream& operator<<(double v)           { ss_ += QString::number(v); return *this; }
    LogStream& operator<<(bool v)             { ss_ += v ? "true" : "false"; return *this; }

private:
    Logger::Level lv_;
    const char*   file_;
    int           line_;
    const char*   func_;
    QString       ss_;
};

// 便捷宏
#define TLOG_DEBUG() LogStream(Logger::DEBUG_L, __FILE__, __LINE__, __FUNCTION__)
#define TLOG_INFO()  LogStream(Logger::INFO_L,  __FILE__, __LINE__, __FUNCTION__)
#define TLOG_WARN()  LogStream(Logger::WARN_L,  __FILE__, __LINE__, __FUNCTION__)
#define TLOG_ERROR() LogStream(Logger::ERROR_L, __FILE__, __LINE__, __FUNCTION__)
