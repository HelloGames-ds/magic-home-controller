#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace elkbledom::AppLog {

inline constexpr qsizetype BufferMax = 2000;

struct Snapshot
{
    QStringList lines;
    quint64 sequence = 0;
};

// Enables or disables normal log writes. The state-change message itself is
// always recorded, matching the Python implementation.
void setEnabled(bool enabled);
[[nodiscard]] bool isEnabled() noexcept;

// Writes an already formatted line.
void log(const QString& text);

// Writes a line prefixed with the current local time ([HH:mm:ss]).
void logline(const QString& text);
void logline(const QStringList& parts);

// A snapshot couples the ring contents with a monotonically increasing
// sequence. Consumers must compare sequence rather than the number of lines:
// once the ring reaches BufferMax its size no longer changes.
[[nodiscard]] Snapshot snapshot();
[[nodiscard]] QStringList buffer();
[[nodiscard]] quint64 sequence();

// Lazily creates %APPDATA%/Magic Home Controller/last.log and returns its path, or an
// empty string when APPDATA/the file is unavailable.
[[nodiscard]] QString filePath();

// Clears only the in-memory ring. The on-disk append-only log is retained.
void clear();

// Python-port naming aliases kept for straightforward call-site migration.
inline void set_enabled(const bool enabled) { setEnabled(enabled); }
[[nodiscard]] inline bool is_enabled() noexcept { return isEnabled(); }
[[nodiscard]] inline QStringList get_buffer() { return buffer(); }
[[nodiscard]] inline QString get_file_path() { return filePath(); }
inline void clear_buffer() { clear(); }

} // namespace elkbledom::AppLog
