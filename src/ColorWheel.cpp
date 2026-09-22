#include "ColorWheel.h"

#include <QBrush>
#include <QConicalGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QRadialGradient>
#include <QSizePolicy>

#include <algorithm>
#include <cmath>


namespace elkbledom {

namespace {
constexpr qreal pi = 3.14159265358979323846;
}

ColorWheel::ColorWheel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(140, 140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
}

void ColorWheel::setRgb(int red, int green, int blue)
{
    const QColor color(std::clamp(red, 0, 255),
                       std::clamp(green, 0, 255),
                       std::clamp(blue, 0, 255));
    if (color.value() <= 0)
        return;

    const qreal colorHue = color.hsvHueF();
    inputHue_ = colorHue >= 0.0 ? colorHue : inputHue_;
    inputSaturation_ = color.hsvSaturationF();
    updateVisualFromColor(color);
}

void ColorWheel::setVisualRgb(int red, int green, int blue)
{
    const QColor color(std::clamp(red, 0, 255),
                       std::clamp(green, 0, 255),
                       std::clamp(blue, 0, 255));
    if (color.value() > 0)
        updateVisualFromColor(color);
}

void ColorWheel::updateVisualFromColor(const QColor& color)
{
    const qreal colorHue = color.hsvHueF();
    const qreal newHue = colorHue >= 0.0 ? colorHue : visualHue_;
    const qreal newSaturation = color.hsvSaturationF();
    if (std::abs(newHue - visualHue_) > 0.002
        || std::abs(newSaturation - visualSaturation_) > 0.005) {
        visualHue_ = newHue;
        visualSaturation_ = newSaturation;
        update();
    }
}

QColor ColorWheel::rgb() const
{
    return QColor::fromHsvF(inputHue_, inputSaturation_, 1.0);
}

ColorWheel::WheelGeometry ColorWheel::wheelGeometry() const noexcept
{
    const int side = std::min(width(), height());
    return {(width() - side) / 2, (height() - side) / 2, side};
}

QImage ColorWheel::buildCache(int side)
{
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    QConicalGradient hueGradient(side / 2.0, side / 2.0, 90.0);
    hueGradient.setColorAt(0.0, QColor(255, 0, 0));
    hueGradient.setColorAt(1.0 / 6.0, QColor(255, 0, 255));
    hueGradient.setColorAt(2.0 / 6.0, QColor(0, 0, 255));
    hueGradient.setColorAt(3.0 / 6.0, QColor(0, 255, 255));
    hueGradient.setColorAt(4.0 / 6.0, QColor(0, 255, 0));
    hueGradient.setColorAt(5.0 / 6.0, QColor(255, 255, 0));
    hueGradient.setColorAt(1.0, QColor(255, 0, 0));
    painter.setBrush(QBrush(hueGradient));
    painter.drawEllipse(0, 0, side, side);

    QRadialGradient saturationGradient(side / 2.0, side / 2.0, side / 2.0);
    saturationGradient.setColorAt(0.0, QColor(255, 255, 255, 255));
    saturationGradient.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setBrush(QBrush(saturationGradient));
    painter.drawEllipse(0, 0, side, side);

    return image;
}

void ColorWheel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    const WheelGeometry wheel = wheelGeometry();
    if (wheel.side <= 0)
        return;

    if (cache_.isNull() || cacheSide_ != wheel.side) {
        cache_ = buildCache(wheel.side);
        cacheSide_ = wheel.side;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawImage(wheel.x, wheel.y, cache_);

    if (!isEnabled()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(30, 30, 36, 160));
        painter.drawEllipse(wheel.x, wheel.y, wheel.side, wheel.side);
        return;
    }

    const qreal centerX = wheel.x + wheel.side / 2.0;
    const qreal centerY = wheel.y + wheel.side / 2.0;
    const qreal angle = visualHue_ * 2.0 * pi;
    const qreal radius = visualSaturation_ * std::max(0.0, wheel.side / 2.0 - 3.0);
    const QPointF marker(centerX + radius * std::sin(angle),
                         centerY - radius * std::cos(angle));

    painter.setPen(QPen(QColor(0, 0, 0, 200), 3));
    painter.setBrush(Qt::white);
    painter.drawEllipse(marker, 8.0, 8.0);

    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(marker, 8.0, 8.0);
}

void ColorWheel::handlePosition(const QPointF& position)
{
    const WheelGeometry wheel = wheelGeometry();
    const qreal maxRadius = wheel.side / 2.0;
    if (maxRadius <= 0.0)
        return;

    const qreal centerX = wheel.x + maxRadius;
    const qreal centerY = wheel.y + maxRadius;
    const qreal dx = position.x() - centerX;
    const qreal dy = position.y() - centerY;

    inputSaturation_ = std::min<qreal>(1.0, std::hypot(dx, dy) / maxRadius);
    const qreal fullTurn = 2.0 * pi;
    inputHue_ = std::fmod(std::atan2(dx, -dy) / fullTurn + 1.0, 1.0);
    if (visualFollowsInput_) {
        visualHue_ = inputHue_;
        visualSaturation_ = inputSaturation_;
        update();
    }

    const QColor color = rgb();
    if (!emitClock_.isValid() || emitClock_.elapsed() >= 33) {
        emitClock_.restart();
        Q_EMIT colorChanged(color.red(), color.green(), color.blue());
    }
}

void ColorWheel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isEnabled()) {
        dragging_ = true;
        handlePosition(event->position());
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ColorWheel::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_ && isEnabled()) {
        handlePosition(event->position());
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ColorWheel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
        const QColor color = rgb();
        emitClock_.restart();
        Q_EMIT colorChanged(color.red(), color.green(), color.blue());
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

} // namespace elkbledom
