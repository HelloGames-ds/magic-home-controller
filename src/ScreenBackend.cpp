#include "ScreenBackend.h"

namespace elkbledom {

// Базовый вариант: маска игнорируется, цвет берётся по всему прямоугольнику.
// Аппаратные бэкенды переопределяют метод и учитывают маску при усреднении.
QColor ScreenBackend::sampleMask(const QRect& area, const QImage& mask) const
{
    Q_UNUSED(mask)
    return sampleArea(area);
}

} // namespace elkbledom
