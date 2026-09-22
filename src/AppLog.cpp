#include "AppLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <atomic>
#include <cstdio>
#include <deque>

namespace elkbledom::AppLog {
namespace {

QMutex bufferMutex;
std::deque<QString> lines;
quint64 changeSequence = 0;
std::atomic_bool loggingEnabled{true};

QMutex fileMutex;
QString logPath;
QFile logFile;
bool fileInitializationAttempted = false;

void ensureFileLocked()
{
    if (logFile.isOpen() || fileInitializationAttempted) {
        return;
    }
    fileInitializationAttempted = true;

    const QString appData = qEnvironmentVariable("APPDATA");
    if (appData.isEmpty()) {
        return;
    }

    QDir directory(appData);
    if (!directory.mkpath(QStringLiteral("Magic Home Controller"))
        || !directory.cd(QStringLiteral("Magic Home Controller"))) {
        return;
    }

    const QString candidate = directory.filePath(QStringLiteral("last.log"));
    logFile.setFileName(candidate);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        logFile.setFileName(QString());
        return;
    }
    logPath = QDir::toNativeSeparators(candidate);
}

void rawEmit(const QString& text, const bool alsoPrint = true)
{
    {
        const QMutexLocker locker(&bufferMutex);
        if (lines.size() == static_cast<std::size_t>(BufferMax)) {
            lines.pop_front();
        }
        lines.push_back(text);
        ++changeSequence;
    }

    if (alsoPrint) {
        const QByteArray utf8 = text.toUtf8();
        std::fwrite(utf8.constData(), 1, static_cast<std::size_t>(utf8.size()), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }

    const QMutexLocker locker(&fileMutex);
    ensureFileLocked();
    if (logFile.isOpen()) {
        logFile.write(text.toUtf8());
        logFile.write("\n");
        logFile.flush();
    }
}

} // namespace

void setEnabled(const bool enabled)
{
    loggingEnabled.store(enabled, std::memory_order_release);
    rawEmit(enabled ? QStringLiteral("[LOG] logging enabled")
                    : QStringLiteral("[LOG] logging disabled"));
}

bool isEnabled() noexcept
{
    return loggingEnabled.load(std::memory_order_acquire);
}

void log(const QString& text)
{
    if (isEnabled()) {
        rawEmit(text);
    }
}

void logline(const QString& text)
{
    log(QStringLiteral("[%1] %2")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), text));
}

void logline(const QStringList& parts)
{
    logline(parts.join(QLatin1Char(' ')));
}

Snapshot snapshot()
{
    const QMutexLocker locker(&bufferMutex);
    Snapshot result;
    result.lines.reserve(static_cast<qsizetype>(lines.size()));
    for (const QString& line : lines) {
        result.lines.append(line);
    }
    result.sequence = changeSequence;
    return result;
}

QStringList buffer()
{
    return snapshot().lines;
}

quint64 sequence()
{
    const QMutexLocker locker(&bufferMutex);
    return changeSequence;
}

QString filePath()
{
    const QMutexLocker locker(&fileMutex);
    ensureFileLocked();
    return logPath;
}

void clear()
{
    const QMutexLocker locker(&bufferMutex);
    lines.clear();
    ++changeSequence;
}

} // namespace elkbledom::AppLog
