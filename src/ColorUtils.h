#pragma once

#include <algorithm>
#include <cmath>

#include <QColor>

namespace elkbledom::color {

// Ограничивает значение заданным диапазоном с усечением дробной части,
// как int() в исходной реализации на Python.
template <typename T>
constexpr int clamp(T value, int low = 0, int high = 255) noexcept
{
    return std::clamp(static_cast<int>(value), low, high);
}

constexpr double lerp(double a, double b, double factor) noexcept
{
    return a + (b - a) * factor;
}

inline QColor lerpRgb(const QColor& first, const QColor& second, double factor) noexcept
{
    return QColor(clamp(lerp(first.red(), second.red(), factor)),
                  clamp(lerp(first.green(), second.green(), factor)),
                  clamp(lerp(first.blue(), second.blue(), factor)));
}

inline QColor hsvToRgb(double hue, double saturation, double value) noexcept
{
    // QColor округляет каналы, тогда как colorsys + int() их усекает.
    // Явное преобразование сохраняет поведение Python-оригинала 1:1.
    hue -= std::floor(hue);
    saturation = std::clamp(saturation, 0.0, 1.0);
    value = std::clamp(value, 0.0, 1.0);

    if (saturation == 0.0) {
        const int channel = clamp(value * 255.0);
        return QColor(channel, channel, channel);
    }

    const double sector = hue * 6.0;
    const int index = static_cast<int>(sector);
    const double fraction = sector - index;
    const double p = value * (1.0 - saturation);
    const double q = value * (1.0 - saturation * fraction);
    const double t = value * (1.0 - saturation * (1.0 - fraction));

    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    switch (index % 6) {
    case 0: red = value; green = t; blue = p; break;
    case 1: red = q; green = value; blue = p; break;
    case 2: red = p; green = value; blue = t; break;
    case 3: red = p; green = q; blue = value; break;
    case 4: red = t; green = p; blue = value; break;
    default: red = value; green = p; blue = q; break;
    }
    return QColor(clamp(red * 255.0), clamp(green * 255.0), clamp(blue * 255.0));
}

inline void rgbToHsv(const QColor& rgb, double& hue, double& saturation,
                     double& value) noexcept
{
    const double red = rgb.red() / 255.0;
    const double green = rgb.green() / 255.0;
    const double blue = rgb.blue() / 255.0;
    const double maximum = std::max({red, green, blue});
    const double minimum = std::min({red, green, blue});
    const double delta = maximum - minimum;

    value = maximum;
    saturation = maximum == 0.0 ? 0.0 : delta / maximum;
    if (delta == 0.0) {
        hue = 0.0;
    } else if (maximum == red) {
        hue = std::fmod((green - blue) / delta, 6.0) / 6.0;
    } else if (maximum == green) {
        hue = ((blue - red) / delta + 2.0) / 6.0;
    } else {
        hue = ((red - green) / delta + 4.0) / 6.0;
    }
    if (hue < 0.0) {
        hue += 1.0;
    }
}

// Интерполяция HSV по кратчайшему пути оттенка.
inline QColor lerpHsv(const QColor& first, const QColor& second, double factor) noexcept
{
    double h1 = 0.0, s1 = 0.0, v1 = 0.0;
    double h2 = 0.0, s2 = 0.0, v2 = 0.0;
    rgbToHsv(first, h1, s1, v1);
    rgbToHsv(second, h2, s2, v2);

    double deltaHue = h2 - h1;
    if (deltaHue > 0.5) {
        deltaHue -= 1.0;
    } else if (deltaHue < -0.5) {
        deltaHue += 1.0;
    }
    return hsvToRgb(h1 + deltaHue * factor,
                    lerp(s1, s2, factor),
                    lerp(v1, v2, factor));
}

} // namespace elkbledom::color
