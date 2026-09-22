#pragma once

#include <QColor>
#include <QRect>
#include <QSize>
#include <QString>
#include <memory>

#include "ScreenBackend.h"

namespace elkbledom {

// Windows.Graphics.Capture (WinRT) screen-capture backend. Unlike Desktop
// Duplication it keeps delivering frames while another app runs in real
// exclusive fullscreen (e.g. older OpenGL games), because the compositor
// mirrors the content instead of releasing the desktop.
//
// Acquires the monitor automatically via IGraphicsCaptureItemInterop::
// CreateForMonitor (no picker dialog). Requires Windows 11 24H2+.
class WgcCapture final : public ScreenBackend
{
public:
    WgcCapture();
    ~WgcCapture() override;
    WgcCapture(const WgcCapture&) = delete;
    WgcCapture& operator=(const WgcCapture&) = delete;

    bool start(int screenIndex, const QRect& physScreenRect);

    bool active() const noexcept override;
    QSize size() const noexcept override;
    bool frameAvailable() override;
    QColor sampleArea(const QRect& area) const override;
    void releaseFrame() override;
    void shutdown() noexcept override;

    QString name() const override { return QStringLiteral("WGC"); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace elkbledom