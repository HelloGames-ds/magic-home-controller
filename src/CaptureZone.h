#pragma once

#include <QColor>
#include <QImage>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace elkbledom {

// Одна зона захвата Ambilight: произвольная область экрана (полигон или
// прямоугольник), из которой берётся цвет, плюс необязательные маски, которые
// из этой области вычитаются. Все координаты нормализованы (0..1) и не зависят
// от разрешения — при смене экрана зона остаётся на месте.
struct CaptureZone
{
    QString name;
    bool enabled = true;
    double weight = 1.0;  // вклад зоны в общий цвет
    double boost = -1.0;  // <0 — использовать общий boost амбилайта
    double smooth = -1.0; // <0 — использовать общее сглаживание
    double brightness = 1.0; // множитель яркости цвета зоны
    double saturation = 1.0; // множитель насыщенности цвета зоны
    QVector<QPolygonF> areas;
    QVector<QPolygonF> masks;
    // Точки кривых слоя, координаты 0..1, по каналам RGB/R/G/B. Пустой вектор
    // означает «без правки». Применяются к цвету слоя до сложения слоёв.
    QVector<QPointF> curveRgb;
    QVector<QPointF> curveR;
    QVector<QPointF> curveG;
    QVector<QPointF> curveB;

    bool isEmpty() const;
    QRectF bounds() const;
    // Применяет кривые слоя к цвету. Пустые кривые ничего не меняют.
    QColor applyCurves(const QColor& color) const;
    bool hasCurves() const;
    static QVector<QPointF> defaultCurve();
    bool operator==(const CaptureZone& other) const;
    bool operator!=(const CaptureZone& other) const { return !(*this == other); }

    // Растеризует зону в маску покрытия (Format_Alpha8) под размер кадра
    // size. Пиксели с ненулевой альфой входят в зону. Возвращает false, если
    // зона пуста или не помещается в кадр; areaOut получает прямоугольник
    // зоны в пикселях кадра.
    bool rasterize(const QSize& size, QRect* areaOut, QImage* maskOut) const;
};

QString encodeZones(const QVector<CaptureZone>& zones);
QVector<CaptureZone> decodeZones(const QString& text);

} // namespace elkbledom
