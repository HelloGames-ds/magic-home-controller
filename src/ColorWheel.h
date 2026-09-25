#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QWidget>

namespace elkbledom {

class ColorWheel final : public QWidget
{
    Q_OBJECT

public:
    explicit ColorWheel(QWidget* parent = nullptr);

    bool isDragging() const noexcept { return dragging_; }

    void setRgb(int red, int green, int blue);
    void setVisualRgb(int red, int green, int blue);
    void setVisualFollowsInput(bool enabled) noexcept { visualFollowsInput_ = enabled; }
    void setEmitThrottleEnabled(bool enabled) noexcept { emitThrottle_ = enabled; }
    QColor rgb() const;

Q_SIGNALS:
    void colorChanged(int red, int green, int blue);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    struct WheelGeometry {
        int x = 0;
        int y = 0;
        int side = 0;
    };

    WheelGeometry wheelGeometry() const noexcept;
    static QImage buildCache(int side);
    void handlePosition(const QPointF& position);
    void updateVisualFromColor(const QColor& color);

    qreal inputHue_ = 0.83;
    qreal inputSaturation_ = 1.0;
    qreal visualHue_ = 0.83;
    qreal visualSaturation_ = 1.0;
    QImage cache_;
    int cacheSide_ = 0;
    QElapsedTimer emitClock_;
    bool dragging_ = false;
    bool visualFollowsInput_ = false;
    bool emitThrottle_ = true;
};

} // namespace elkbledom
