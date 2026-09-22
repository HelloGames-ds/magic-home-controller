#pragma once

#include <QColor>
#include <QSize>
#include <QRect>
#include <memory>

#include "ScreenBackend.h"

namespace elkbledom {

// Hardware video-frame capture via DXGI Desktop Duplication (Windows only).
// Grabs the selected monitor and lets the caller sample average colors of
// arbitrary rectangles directly from the mapped GPU buffer — no intermediate
// QImage is ever built. Falls back to GDI in the caller when init fails.
class DxgiCapture final : public ScreenBackend
{
public:
    DxgiCapture();
    ~DxgiCapture() override;
    DxgiCapture(const DxgiCapture&) = delete;
    DxgiCapture& operator=(const DxgiCapture&) = delete;

    // Binds the duplication to the monitor that intersects physScreenRect.
    bool start(int screenIndex, const QRect& physScreenRect);

    bool active() const noexcept override;

    QSize size() const noexcept override;
    int width() const noexcept;
    int height() const noexcept;

    bool frameAvailable() override;

    QColor sampleArea(const QRect& area) const override;

    void releaseFrame() override;
    void shutdown() noexcept override;

    QString name() const override { return QStringLiteral("DXGI"); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace elkbledom