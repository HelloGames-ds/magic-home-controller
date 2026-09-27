#include "CaptureZone.h"

#include <cstring>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QSize>
#include <QtMath>

#include <algorithm>

namespace elkbledom {
namespace {

QJsonArray encodePolygon(const QPolygonF& polygon)
{
    QJsonArray points;
    for (const QPointF& point : polygon) {
        QJsonArray pair;
        pair.append(point.x());
        pair.append(point.y());
        points.append(pair);
    }
    return points;
}

QPolygonF decodePolygon(const QJsonValue& value)
{
    QPolygonF polygon;
    const QJsonArray points = value.toArray();
    for (const QJsonValue& entry : points) {
        const QJsonArray pair = entry.toArray();
        if (pair.size() >= 2)
            polygon.append(QPointF(pair.at(0).toDouble(), pair.at(1).toDouble()));
    }
    return polygon;
}

QPolygonF polygonToMask(const QPolygonF& polygon, const QSize& frame, const QPoint& origin)
{
    QPolygonF result;
    result.reserve(polygon.size());
    for (const QPointF& point : polygon) {
        result.append(QPointF(point.x() * frame.width() - origin.x(),
                              point.y() * frame.height() - origin.y()));
    }
    return result;
}

void fillPolygon(QPainter& painter, const QPolygonF& polygon)
{
    if (polygon.size() >= 3) {
        painter.drawPolygon(polygon);
        return;
    }
    if (polygon.size() == 2) {
        const QRectF rect = QRectF(polygon.at(0), polygon.at(1)).normalized();
        painter.drawRect(rect);
    }
}

} // namespace

bool CaptureZone::isEmpty() const
{
    for (const QPolygonF& polygon : areas) {
        if (polygon.size() >= 2) return false;
    }
    return true;
}

QRectF CaptureZone::bounds() const
{
    // QRectF::united() не годится: у вырожденного прямоугольника (одна точка)
    // isNull() == true, и united() просто возвращает второй аргумент, теряя
    // остальные точки. Поэтому границы считаются min/max вручную.
    double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    bool started = false;
    for (const QPolygonF& polygon : areas) {
        for (const QPointF& point : polygon) {
            if (!started) {
                minX = maxX = point.x();
                minY = maxY = point.y();
                started = true;
                continue;
            }
            minX = std::min(minX, point.x());
            maxX = std::max(maxX, point.x());
            minY = std::min(minY, point.y());
            maxY = std::max(maxY, point.y());
        }
    }
    if (!started) return {};
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

bool CaptureZone::operator==(const CaptureZone& other) const
{
    return name == other.name && enabled == other.enabled
        && qFuzzyCompare(weight + 1.0, other.weight + 1.0)
        && qFuzzyCompare(boost + 2.0, other.boost + 2.0)
        && qFuzzyCompare(smooth + 2.0, other.smooth + 2.0)
        && qFuzzyCompare(brightness + 1.0, other.brightness + 1.0)
        && qFuzzyCompare(saturation + 1.0, other.saturation + 1.0)
        && areas == other.areas && masks == other.masks;
}

bool CaptureZone::rasterize(const QSize& size, QRect* areaOut, QImage* maskOut) const
{
    const QRectF unit = bounds();
    if (size.width() < 1 || size.height() < 1 || unit.isEmpty()) return false;

    const QRectF clamped = unit.intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (clamped.isEmpty()) return false;

    const int x = std::clamp(int(std::floor(clamped.x() * size.width())), 0, size.width() - 1);
    const int y = std::clamp(int(std::floor(clamped.y() * size.height())), 0, size.height() - 1);
    const int right = std::clamp(int(std::ceil(clamped.right() * size.width())), 1, size.width());
    const int bottom = std::clamp(int(std::ceil(clamped.bottom() * size.height())), 1, size.height());
    if (right <= x || bottom <= y) return false;

    // Зона рисуется в пикселях кадра, а маска хранит только её bbox, поэтому
    // перед отрисовкой координаты сдвигаются на начало прямоугольника.
    const QRect box(x, y, right - x, bottom - y);

    QImage mask(box.size(), QImage::Format_Alpha8);
    mask.fill(0);
    {
        QPainter painter(&mask);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(Qt::NoPen);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setBrush(QColor(255, 255, 255));
        for (const QPolygonF& polygon : areas) {
            if (polygon.size() < 2) continue;
            fillPolygon(painter, polygonToMask(polygon, size, box.topLeft()));
        }
        if (!masks.isEmpty()) {
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.setBrush(QColor(0, 0, 0));
            for (const QPolygonF& polygon : masks) {
                if (polygon.size() < 2) continue;
                fillPolygon(painter, polygonToMask(polygon, size, box.topLeft()));
            }
        }
    }
    if (maskOut) *maskOut = mask;
    if (areaOut) *areaOut = box;
    return true;
}

QVector<QPointF> CaptureZone::defaultCurve()
{
    return { QPointF(0.0, 0.0), QPointF(1.0, 1.0) };
}

bool CaptureZone::hasCurves() const
{
    return !curveRgb.isEmpty() || !curveR.isEmpty() || !curveG.isEmpty() || !curveB.isEmpty();
}

namespace {

// Кривая по точкам в таблицу 256 значений. Куски между точками считаются
// линейно, но непрерывно: Photoshop так же ведёт себя на глаз, плюс таблица
// всегда строго неубывает — иначе цвета «инвертировались» бы на резких
// участках.
QVector<int> buildCurveLut(const QVector<QPointF>& points)
{
    QVector<int> lut(256);
    if (points.size() < 2) {
        for (int i = 0; i < 256; ++i) lut[i] = i;
        return lut;
    }
    QVector<QPointF> sorted = points;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });
    int at = 0;
    for (int i = 0; i < 256; ++i) {
        const double x = i / 255.0;
        while (at + 2 < sorted.size() && sorted.at(at + 1).x() < x) ++at;
        const int last = static_cast<int>(sorted.size()) - 1;
        const QPointF& a = sorted.at(at);
        const QPointF& b = sorted.at(std::min(at + 1, last));
        const double span = b.x() - a.x();
        const double t = span > 1e-9 ? std::clamp((x - a.x()) / span, 0.0, 1.0) : 0.0;
        const double y = a.y() + (b.y() - a.y()) * t;
        lut[i] = std::clamp(int(y * 255.0 + 0.5), 0, 255);
    }
    for (int i = 1; i < 256; ++i) lut[i] = std::max(lut[i], lut[i - 1]);
    return lut;
}

// Возвращаем копию: следующий insert может перехешировать QHash и инвалидировать
// ссылку на прежнее значение.
QVector<int> cachedLut(const QVector<QPointF>& points)
{
    static QHash<quint64, QVector<int>> cache;
    // Свой хеш точек: qHash для QPointF в Qt 6.8 не подходит для ключа.
    quint64 key = 1469598103934665603ull;
    for (const QPointF& point : points) {
        double xs = point.x(), ys = point.y();
        quint64 xb = 0, yb = 0;
        std::memcpy(&xb, &xs, sizeof(xb));
        std::memcpy(&yb, &ys, sizeof(yb));
        key = (key ^ xb) * 1099511628211ull;
        key = (key ^ yb) * 1099511628211ull;
    }
    const int count = static_cast<int>(points.size());
    key = (key ^ quint64(count)) * 1099511628211ull;
    auto it = cache.find(key);
    if (it != cache.end()) return it.value();
    if (cache.size() > 64) cache.clear();
    cache.insert(key, buildCurveLut(points));
    return cache.value(key);
}

} // namespace

QColor CaptureZone::applyCurves(const QColor& color) const
{
    if (!color.isValid() || !hasCurves()) return color;
    // Общая криная RGB применяется ко всем каналам, канальная — поверх неё,
    // как в Photoshop.
    const auto one = [this](int value, const QVector<QPointF>& specific) {
        const QVector<QPointF>& points = !specific.isEmpty() ? specific : curveRgb;
        if (points.isEmpty()) return value;
        const QVector<int> lut = cachedLut(points);
        return std::clamp(lut.at(std::clamp(value, 0, 255)), 0, 255);
    };
    return QColor(one(color.red(), curveR), one(color.green(), curveG), one(color.blue(), curveB));
}

QString encodeZones(const QVector<CaptureZone>& zones)
{
    QJsonArray array;
    for (const CaptureZone& zone : zones) {
        QJsonArray areas;
        for (const QPolygonF& polygon : zone.areas) areas.append(encodePolygon(polygon));
        QJsonArray masks;
        for (const QPolygonF& polygon : zone.masks) masks.append(encodePolygon(polygon));
        QJsonObject object;
        object.insert(QStringLiteral("name"), zone.name);
        object.insert(QStringLiteral("enabled"), zone.enabled);
        object.insert(QStringLiteral("weight"), zone.weight);
        object.insert(QStringLiteral("boost"), zone.boost);
        object.insert(QStringLiteral("smooth"), zone.smooth);
        object.insert(QStringLiteral("brightness"), zone.brightness);
        object.insert(QStringLiteral("saturation"), zone.saturation);
        object.insert(QStringLiteral("areas"), areas);
        object.insert(QStringLiteral("masks"), masks);
        const auto encodeCurve = [](const QVector<QPointF>& points) {
            QJsonArray array;
            for (const QPointF& point : points)
                array.append(QJsonArray{ point.x(), point.y() });
            return array;
        };
        if (zone.hasCurves()) {
            object.insert(QStringLiteral("curveRgb"), encodeCurve(zone.curveRgb));
            object.insert(QStringLiteral("curveR"), encodeCurve(zone.curveR));
            object.insert(QStringLiteral("curveG"), encodeCurve(zone.curveG));
            object.insert(QStringLiteral("curveB"), encodeCurve(zone.curveB));
        }
        array.append(object);
    }
    if (array.isEmpty()) return {};
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVector<CaptureZone> decodeZones(const QString& text)
{
    QVector<CaptureZone> zones;
    if (text.trimmed().isEmpty()) return zones;
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) return zones;
    const QJsonArray array = document.array();
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        CaptureZone zone;
        zone.name = object.value(QStringLiteral("name")).toString();
        zone.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        zone.weight = object.value(QStringLiteral("weight")).toDouble(1.0);
        zone.boost = object.value(QStringLiteral("boost")).toDouble(-1.0);
        zone.smooth = object.value(QStringLiteral("smooth")).toDouble(-1.0);
        zone.brightness = std::clamp(object.value(QStringLiteral("brightness")).toDouble(1.0), 0.0, 2.0);
        zone.saturation = std::clamp(object.value(QStringLiteral("saturation")).toDouble(1.0), 0.0, 2.0);
        for (const QJsonValue& entry : object.value(QStringLiteral("areas")).toArray()) {
            const QPolygonF polygon = decodePolygon(entry);
            if (polygon.size() >= 2) zone.areas.push_back(polygon);
        }
        const auto decodeCurve = [](const QJsonValue& value) {
            QVector<QPointF> points;
            for (const QJsonValue& entry : value.toArray()) {
                const QJsonArray pair = entry.toArray();
                if (pair.size() < 2) continue;
                points.append(QPointF(pair.at(0).toDouble(), pair.at(1).toDouble()));
            }
            return points;
        };
        zone.curveRgb = decodeCurve(object.value(QStringLiteral("curveRgb")));
        zone.curveR = decodeCurve(object.value(QStringLiteral("curveR")));
        zone.curveG = decodeCurve(object.value(QStringLiteral("curveG")));
        zone.curveB = decodeCurve(object.value(QStringLiteral("curveB")));
        for (const QJsonValue& entry : object.value(QStringLiteral("masks")).toArray()) {
            const QPolygonF polygon = decodePolygon(entry);
            if (polygon.size() >= 2) zone.masks.push_back(polygon);
        }
        // Пустые зоны не выбрасываем: только что добавленная зона без формы должна
        // пережить сохранение и повторное открытие диалога. На захват они не влияют —
        // hasZones() и все циклы выборки пропускают зоны без геометрии.
        if (!zone.name.isEmpty() || !zone.areas.isEmpty() || !zone.masks.isEmpty())
            zones.push_back(zone);
    }
    return zones;
}

} // namespace elkbledom
