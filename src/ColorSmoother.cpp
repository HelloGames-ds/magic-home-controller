#include "ColorSmoother.h"

#include "ColorUtils.h"

#include <algorithm>
#include <cmath>

namespace elkbledom {

ColorSmoother::ColorSmoother(QObject* parent, int intervalMs, int tauMs)
    : QObject(parent)
    , intervalMs_(std::max(1, intervalMs))
    , tauSeconds_(std::max(0.02, tauMs / 1000.0))
{
    timer_.setInterval(intervalMs_);
    timer_.setTimerType(Qt::CoarseTimer);
    connect(&timer_, &QTimer::timeout, this, &ColorSmoother::tick);
}

void ColorSmoother::setEnabled(bool enabled)
{
    enabled_ = enabled;
    if (!enabled_) {
        timer_.stop();
        current_ = target_;
        lastEmitted_ = current_;
        hasLastEmitted_ = true;
        Q_EMIT colorChanged(current_.red(), current_.green(), current_.blue());
    }
}

void ColorSmoother::setTau(int tauMs)
{
    tauSeconds_ = std::max(0.02, tauMs / 1000.0);
}

void ColorSmoother::setCurrent(const QColor& rgb)
{
    if (!rgb.isValid()) {
        return;
    }
    current_ = rgb.toRgb();
    target_ = current_;
    lastEmitted_ = current_;
    hasLastEmitted_ = true;
    timer_.stop();
}

void ColorSmoother::setCurrent(int red, int green, int blue)
{
    setCurrent(QColor(red, green, blue));
}

void ColorSmoother::animateTo(const QColor& rgb)
{
    if (!rgb.isValid()) {
        return;
    }
    const QColor next = rgb.toRgb();
    if (!enabled_) {
        current_ = next;
        target_ = next;
        lastEmitted_ = next;
        hasLastEmitted_ = true;
        Q_EMIT colorChanged(next.red(), next.green(), next.blue());
        return;
    }

    target_ = next;
    // В Python одинаковая цель игнорировалась даже после остановки таймера.
    // Если current ещё не дошёл до неё, это навсегда замораживало переход.
    if (current_ == target_) {
        timer_.stop();
        return;
    }
    if (!timer_.isActive()) {
        timer_.start();
    }
}

void ColorSmoother::animateTo(int red, int green, int blue)
{
    animateTo(QColor(red, green, blue));
}

void ColorSmoother::tick()
{
    const double factor = 1.0 - std::exp(-(intervalMs_ / 1000.0) / tauSeconds_);
    QColor next = color::lerpHsv(current_, target_, factor);
    const int distance = std::abs(next.red() - target_.red())
                       + std::abs(next.green() - target_.green())
                       + std::abs(next.blue() - target_.blue());

    // HSV-интерполяция с усечением иногда даёт тот же RGB и не приближается
    // к цели. Продвигаем каждый застрявший канал минимум на единицу.
    if (next == current_ && next != target_) {
        auto step = [](int value, int target) {
            return value + (target > value ? 1 : (target < value ? -1 : 0));
        };
        next = QColor(step(current_.red(), target_.red()),
                      step(current_.green(), target_.green()),
                      step(current_.blue(), target_.blue()));
    }

    if (distance <= 2 || next == target_) {
        next = target_;
        timer_.stop();
    }
    current_ = next;

    const int changed = hasLastEmitted_
        ? std::max({std::abs(next.red() - lastEmitted_.red()),
                    std::abs(next.green() - lastEmitted_.green()),
                    std::abs(next.blue() - lastEmitted_.blue())})
        : 255;
    if (changed >= 1) {
        lastEmitted_ = next;
        hasLastEmitted_ = true;
        Q_EMIT colorChanged(next.red(), next.green(), next.blue());
    }
}

} // namespace elkbledom
