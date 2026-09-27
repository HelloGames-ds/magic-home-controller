#pragma once

#ifdef Q_OS_WIN

#include <QColor>
#include <QImage>
#include <QRect>
#include <QtGlobal>

#include <algorithm>

namespace elkbledom {

// Усредняет сетку до 8×8 точек внутри area, оставляя только пиксели, покрытые
// маской mask (Format_Alpha8, размером с area; ненулевая альфа = внутри зоны).
// Формат кадра — B8G8R8A8, как в DxgiCapture::sampleArea.
inline QColor averageBgraMasked(const unsigned char* bits, int pitch,
                                const QRect& area, const QImage& mask)
{
    if (!bits || area.width() < 1 || area.height() < 1 || mask.isNull()) {
        return QColor();
    }

    const int width = area.width();
    const int height = area.height();
    const int stepX = qMax(1, width / 8);
    const int stepY = qMax(1, height / 8);
    const int maskWidth = mask.width();
    const int maskHeight = mask.height();

    quint64 red = 0, green = 0, blue = 0;
    quint32 count = 0;
    for (int y = 0; y < height; y += stepY) {
        const int maskY = std::clamp(y * maskHeight / height, 0, maskHeight - 1);
        const auto* maskRow = reinterpret_cast<const unsigned char*>(mask.constScanLine(maskY));
        const unsigned char* row = bits + size_t(area.y() + y) * size_t(pitch);
        for (int x = 0; x < width; x += stepX) {
            const int maskX = std::clamp(x * maskWidth / width, 0, maskWidth - 1);
            if (maskRow[maskX] == 0) continue;
            const unsigned char* pixel = row + size_t(area.x() + x) * 4u;
            red += pixel[2];
            green += pixel[1];
            blue += pixel[0];
            ++count;
        }
    }
    if (count == 0) return QColor();
    return QColor(int(red / count), int(green / count), int(blue / count));
}

} // namespace elkbledom

#endif // Q_OS_WIN
