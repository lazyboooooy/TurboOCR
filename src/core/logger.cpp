#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <cstdio>
#include <iostream>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const QString& logPath) {
    QMutexLocker lock(&mutex_);
    if (inited_) return;

    QFileInfo fi(logPath);
    QDir logDir = fi.absoluteDir();
    QString baseName = fi.completeBaseName();  // "turboocr"
    logDir.mkpath(".");

    // ---- 清理超过 3 天的旧日志 ----
    QDate today = QDate::currentDate();
    QStringList oldLogs = logDir.entryList(
        {baseName + "_*.log"}, QDir::Files, QDir::Name);
    for (const auto& name : oldLogs) {
        // 从文件名提取日期: turboocr_2026-07-12.log
        QString dateStr = name.mid(baseName.size() + 1, 10);  // "2026-07-12"
        QDate fileDate = QDate::fromString(dateStr, "yyyy-MM-dd");
        if (fileDate.isValid() && fileDate.daysTo(today) > 3) {
            QFile::remove(logDir.filePath(name));
        }
    }

    // ---- 当日日志文件 ----
    QString todayLog = logDir.filePath(
        baseName + "_" + today.toString("yyyy-MM-dd") + ".log");
    file_.setFileName(todayLog);
    if (file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        inited_ = true;
        QString sep = QString(60, QChar('-')) + "\n";
        file_.write(sep.toUtf8());
        QString startMsg = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss]")
                         + " [INFO ] TurboOCR log started\n";
        file_.write(startMsg.toUtf8());
        file_.flush();
    }
}

const char* Logger::levelStr(Level lv) {
    switch (lv) {
        case DEBUG_L: return "DEBUG";
        case INFO_L:  return "INFO ";
        case WARN_L:  return "WARN ";
        case ERROR_L: return "ERROR";
    }
    return "?????";
}

void Logger::write(Level lv, const char* file, int line, const char* func, const QString& msg) {
    if (lv < level_) return;

    // 提取文件名（去掉路径）
    const char* base = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }

    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString lineStr = QString("[%1] [%2] %3:%4 %5()] %6\n")
        .arg(ts)
        .arg(levelStr(lv))
        .arg(QString::fromUtf8(base))
        .arg(line)
        .arg(QString::fromUtf8(func))
        .arg(msg);

    QMutexLocker lock(&mutex_);

    // 输出到 stderr
    fputs(qPrintable(lineStr), stderr);
    fflush(stderr);

    // 输出到文件
    if (inited_ && file_.isOpen()) {
        file_.write(lineStr.toUtf8());
        file_.flush();
    }
}

LogStream::~LogStream() {
    Logger::instance().write(lv_, file_, line_, func_, ss_);
}
