#include "EffectEngine.h"

#include "ColorUtils.h"

#include <algorithm>
#include <cmath>

#include <QRandomGenerator>

namespace elkbledom {
namespace {

constexpr double pi = 3.14159265358979323846;

double randomUnit()
{
    return QRandomGenerator::global()->generateDouble();
}

int randomInclusive(int low, int high)
{
    return QRandomGenerator::global()->bounded(low, high + 1);
}

} // namespace

EffectEngine::EffectEngine(QObject* parent)
    : QObject(parent)
{
    timer_.setInterval(100);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &EffectEngine::tick);
}

const QStringList& EffectEngine::effects()
{
    static const QStringList value{
        QStringLiteral("static"), QStringLiteral("breath"),
        QStringLiteral("rainbow"), QStringLiteral("gradient"),
        QStringLiteral("strobe"), QStringLiteral("pulse"),
        QStringLiteral("wave"), QStringLiteral("fire"),
        QStringLiteral("random_flash"), QStringLiteral("chase"),
        QStringLiteral("color_cycle")};
    return value;
}

const QHash<QString, QString>& EffectEngine::labels()
{
    static const QHash<QString, QString> value{
        {QStringLiteral("static"), tr("Static")},
        {QStringLiteral("breath"), tr("Breath")},
        {QStringLiteral("rainbow"), tr("Rainbow (HSV)")},
        {QStringLiteral("gradient"), tr("Gradient")},
        {QStringLiteral("strobe"), tr("Strobe")},
        {QStringLiteral("pulse"), tr("Pulse")},
        {QStringLiteral("wave"), tr("Wave")},
        {QStringLiteral("fire"), tr("Fire")},
        {QStringLiteral("random_flash"), tr("Random flashes")},
        {QStringLiteral("chase"), tr("Chasing light")},
        {QStringLiteral("color_cycle"), tr("Palette cycle")}};
    return value;
}

const QHash<QString, QString>& EffectEngine::descriptions()
{
    static const QHash<QString, QString> value{
        {QStringLiteral("static"), tr("Solid color. Uses Color 1 of the palette (primary).")},
        {QStringLiteral("breath"), tr("Smooth fade-out and fade-in of Color 1.")},
        {QStringLiteral("rainbow"), tr("Full HSV cycle. The palette is not used.")},
        {QStringLiteral("gradient"), tr("Smooth transition between Color 1 and Color 2 of the palette.")},
        {QStringLiteral("strobe"), tr("Rapid flashing with Color 1 (on/off).")},
        {QStringLiteral("pulse"), tr("Decaying pulse of Color 1.")},
        {QStringLiteral("wave"), tr("Wave-like sweep across the whole palette (ease-in-out).")},
        {QStringLiteral("fire"), tr("Random flashes of palette colors, imitating fire.")},
        {QStringLiteral("random_flash"), tr("Random palette color, changing ~4 times per second.")},
        {QStringLiteral("chase"), tr("Fast circular cycling of palette colors (4 steps per second).")},
        {QStringLiteral("color_cycle"), tr("Smooth cycle across all palette colors.")}};
    return value;
}

const QHash<QString, int>& EffectEngine::paletteUsage()
{
    static const QHash<QString, int> value{
        {QStringLiteral("static"), 1}, {QStringLiteral("breath"), 1},
        {QStringLiteral("rainbow"), 0}, {QStringLiteral("gradient"), 2},
        {QStringLiteral("strobe"), 1}, {QStringLiteral("pulse"), 1},
        {QStringLiteral("wave"), -1}, {QStringLiteral("fire"), -1},
        {QStringLiteral("random_flash"), -1}, {QStringLiteral("chase"), -1},
        {QStringLiteral("color_cycle"), -1}};
    return value;
}

void EffectEngine::setEffect(const QString& name)
{
    effect_ = name;
    hasLastColor_ = false;
}

void EffectEngine::setPalette(const QList<QColor>& palette)
{
    palette_.clear();
    for (const QColor& color : palette) {
        if (color.isValid()) {
            palette_.append(color.toRgb());
        }
    }
    if (palette_.isEmpty()) {
        palette_.append(QColor(191, 0, 255));
    }
}

void EffectEngine::setSpeed(double speed) { speed_ = std::clamp(speed, 0.05, 10.0); }
void EffectEngine::setIntensity(double intensity) { intensity_ = std::clamp(intensity, 0.1, 1.0); }
void EffectEngine::setNoise(double noise) { noise_ = std::clamp(noise, 0.0, 1.0); }
void EffectEngine::setReverse(bool reverse) { reverse_ = reverse; }
void EffectEngine::setInterval(int milliseconds) { timer_.setInterval(std::max(20, milliseconds)); }

void EffectEngine::start()
{
    elapsed_.restart();
    hasLastColor_ = false;
    timer_.start();
}

void EffectEngine::stop() { timer_.stop(); }

QColor EffectEngine::interpolate(const QColor& first, const QColor& second,
                                 double factor)
{
    return color::lerpRgb(first, second, factor);
}

QColor EffectEngine::paletteAt(double position) const
{
    const int count = palette_.size();
    if (count == 1) {
        return palette_.first();
    }
    position -= std::floor(position);
    const double segment = position * count;
    const int index = static_cast<int>(segment);
    return interpolate(palette_[index % count], palette_[(index + 1) % count],
                       segment - index);
}

void EffectEngine::tick()
{
    if (!elapsed_.isValid()) {
        elapsed_.start();
    }
    double time = elapsed_.elapsed() / 1000.0 * speed_;
    if (reverse_) {
        time = -time;
    }
    const int count = palette_.size();
    QColor raw;

    if (effect_ == QStringLiteral("static")) {
        raw = palette_.first();
    } else if (effect_ == QStringLiteral("breath")) {
        double factor = (std::sin(time * 1.5) + 1.0) / 2.0;
        factor = 0.12 + 0.88 * factor;
        raw = QColor(color::clamp(palette_.first().red() * factor),
                     color::clamp(palette_.first().green() * factor),
                     color::clamp(palette_.first().blue() * factor));
    } else if (effect_ == QStringLiteral("rainbow")) {
        double hue = std::fmod(time * 0.15, 1.0);
        if (hue < 0.0) hue += 1.0;
        // Исходник здесь использует QColor.fromHsvF, включая его округление.
        raw = QColor::fromHsvF(hue, 1.0, 1.0);
    } else if (effect_ == QStringLiteral("gradient")) {
        const double factor = (std::sin(time * 0.8) + 1.0) / 2.0;
        raw = interpolate(palette_.first(), palette_[1 % count], factor);
    } else if (effect_ == QStringLiteral("strobe")) {
        raw = std::sin(time * 8.0) > 0.0 ? palette_.first() : QColor(0, 0, 0);
    } else if (effect_ == QStringLiteral("pulse")) {
        double phase = std::fmod(time * 1.2, 1.0);
        if (phase < 0.0) phase += 1.0;
        const double factor = std::exp(-phase * 6.0);
        raw = QColor(color::clamp(palette_.first().red() * factor),
                     color::clamp(palette_.first().green() * factor),
                     color::clamp(palette_.first().blue() * factor));
    } else if (effect_ == QStringLiteral("wave")) {
        double factor = (std::sin(time * 2.0) + 1.0) / 2.0;
        factor = factor * factor * (3.0 - 2.0 * factor);
        raw = paletteAt(factor);
    } else if (effect_ == QStringLiteral("fire")) {
        const QColor base = paletteAt(randomUnit());
        const double factor = 0.4 + 0.6 * randomUnit();
        raw = QColor(color::clamp(base.red() * factor),
                     color::clamp(base.green() * factor),
                     color::clamp(base.blue() * factor));
    } else if (effect_ == QStringLiteral("random_flash")) {
        const qint64 now = elapsed_.elapsed();
        if (!flashColor_.isValid() || now - flashTimeMs_ > 250) {
            if (randomUnit() < 0.4) {
                flashColor_ = palette_[randomInclusive(0, count - 1)];
                flashTimeMs_ = now;
            }
        }
        raw = flashColor_.isValid() ? flashColor_ : palette_.first();
    } else if (effect_ == QStringLiteral("chase")) {
        int index = static_cast<int>(std::floor(time * 4.0));
        index %= count;
        if (index < 0) index += count;
        raw = palette_[index];
    } else if (effect_ == QStringLiteral("color_cycle")) {
        double position = std::fmod(time * 0.2, 1.0);
        if (position < 0.0) position += 1.0;
        raw = paletteAt(position);
    } else {
        raw = palette_.first();
    }

    if (noise_ > 0.0) {
        const int delta = static_cast<int>(noise_ * 60.0);
        raw = QColor(std::clamp(raw.red() + randomInclusive(-delta, delta), 0, 255),
                     std::clamp(raw.green() + randomInclusive(-delta, delta), 0, 255),
                     std::clamp(raw.blue() + randomInclusive(-delta, delta), 0, 255));
    }
    if (intensity_ < 1.0) {
        raw = QColor(color::clamp(raw.red() * intensity_),
                     color::clamp(raw.green() * intensity_),
                     color::clamp(raw.blue() * intensity_));
    }
    emitIfChanged(raw);
}

void EffectEngine::emitIfChanged(const QColor& color)
{
    if (hasLastColor_ && color == lastColor_) {
        return;
    }
    lastColor_ = color;
    hasLastColor_ = true;
    Q_EMIT colorChanged(color.red(), color.green(), color.blue());
}

} // namespace elkbledom
