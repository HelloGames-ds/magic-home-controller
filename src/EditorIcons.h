#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace elkbledom {

// Тонкие линейные иконки в духе Photoshop: рисуются векторно через QPainter,
// поэтому остаются резкими на любом DPI и не требуют файлов ресурсов.
namespace icons {

enum class Id {
    Select,        // стрелка выделения
    Lasso,         // лассо
    Rectangle,     // прямоугольная область
    CutLasso,      // вычитание лассо
    CutRectangle,  // вычитание прямоугольником
    Pan,           // рука
    ZoomIn,
    ZoomOut,
    Fit,           // вписать в окно
    Grid,          // сетка
    Split,         // до/после
    Fullscreen,
    Capture,       // снимок экрана
    Open,          // открыть файл
    Trash,         // удалить
    MirrorX,
    MirrorY,
    Rotate,
    Plus,          // добавить зону
    Camera,        // зона из окна
    Duplicate,     // дублировать зону
    ArrowUp,
    ArrowDown,
    Undo,
    Redo,
    PanelLeft,     // показать/скрыть левую панель
    PanelRight,    // показать/скрыть правую панель
};

QIcon make(Id id, const QColor& color = QColor(228, 228, 236), int size = 20);

// Отдельная иконка для выбранного состояния (обычно акцентный цвет).
QIcon makeActive(Id id, const QColor& color, int size = 20);

} // namespace icons
} // namespace elkbledom
