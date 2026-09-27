#pragma once

#include <QColor>
#include <QImage>
#include <QRect>
#include <QSize>
#include <QString>

namespace elkbledom {

// Abstract screen-capture backend for Ambilight. Implementations grab the
// selected monitor into a GPU/mapped buffer and let the caller average colors
// of arbitrary rectangles directly from it (no intermediate QImage).
// DxgiCapture (Desktop Duplication) and WgcCapture (Windows.Graphics.Capture)
// both implement this; GDI is handled in the caller as a plain fallback.
class ScreenBackend
{
public:
    virtual ~ScreenBackend() = default;

    virtual bool active() const noexcept = 0;

    // Acquires the newest frame. Returns false when the screen is unchanged or
    // the capture is unavailable; the frame is retained until releaseFrame().
    // Only one acquired frame may be live at a time.
    virtual bool frameAvailable() = 0;

    // Averages the given physical-pixel area of the retained frame.
    // Returns an invalid QColor if no frame is acquired or on bad format.
    virtual QColor sampleArea(const QRect& area) const = 0;

    // Averages the same 8×8 sample grid, but keeps only the points covered by
    // mask (Format_Alpha8, same size as area, non-zero = inside the zone).
    // This is what polygon zones use. The default implementation ignores the
    // mask, so backends without mask support still return a usable color.
    virtual QColor sampleMask(const QRect& area, const QImage& mask) const;

    virtual void releaseFrame() = 0;

    virtual QSize size() const noexcept = 0;

    // Short backend name for logs ("DXGI", "WGC").
    virtual QString name() const = 0;

    virtual void shutdown() noexcept = 0;
};

} // namespace elkbledom