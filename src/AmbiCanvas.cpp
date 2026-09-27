#include "AmbiCanvas.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <functional>

namespace elkbledom {
namespace {

// Поля вокруг картинки внутри документа.
constexpr int kMargin = 12;
constexpr int kHandleSize = 9;
constexpr int kGrabRadius = 7;
// Минимальный размер слоя в нормализованных координатах.
constexpr double kMinZoneSize = 0.02;

// Цвет границы слоя — фиолетовый пунктир, как в исходной версии.
const QColor kActiveBorder(176, 112, 255);
const QColor kLayerBorder(150, 150, 170);

QPolygonF rectPolygon(const QRectF& rect)
{
    return QPolygonF({rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()});
}

QRectF polygonRect(const QPolygonF& polygon)
{
    if (polygon.isEmpty()) return QRectF();
    double minX = polygon.at(0).x(), maxX = minX;
    double minY = polygon.at(0).y(), maxY = minY;
    for (const QPointF& point : polygon) {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

QColor layerColorFor(const QVector<QColor>& colors, int index)
{
    if (index >= 0 && index < colors.size() && colors.at(index).isValid()) return colors.at(index);
    return QColor(140, 140, 160);
}

} // namespace

AmbiCanvas::AmbiCanvas(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    if (auto* area = qobject_cast<QScrollArea*>(parentWidget())) {
        if (QWidget* viewport = area->viewport()) viewport->installEventFilter(this);
    }
}

QScrollArea* AmbiCanvas::scrollArea() const
{
    for (QWidget* parent = parentWidget(); parent; parent = parent->parentWidget()) {
        if (auto* area = qobject_cast<QScrollArea*>(parent)) return area;
    }
    return nullptr;
}

QSize AmbiCanvas::viewportSize() const
{
    if (QScrollArea* area = scrollArea()) return area->viewport()->size();
    return size();
}

QSize AmbiCanvas::sizeHint() const
{
    return documentSize();
}

QSize AmbiCanvas::documentSize() const
{
    if (image_.isNull()) return viewportSize();
    return QSize(qMax(1, qRound(image_.width() * zoom_)) + 2 * kMargin,
                 qMax(1, qRound(image_.height() * zoom_)) + 2 * kMargin);
}

QRectF AmbiCanvas::imageRect() const
{
    const QSize doc = documentSize();
    return QRectF((doc.width() - image_.width() * zoom_) / 2.0,
                  (doc.height() - image_.height() * zoom_) / 2.0,
                  image_.width() * zoom_, image_.height() * zoom_);
}

QRectF AmbiCanvas::selectionPixels() const
{
    const QRectF box = zoneBox(activeZone_);
    if (box.isNull() || box.isEmpty()) return QRectF();
    const QRectF image = imageRect();
    if (image.width() <= 0.0 || image.height() <= 0.0) return QRectF();
    return QRectF((box.x() - (image.left() / image.width())) * image_.width(),
                  (box.y() - (image.top() / image.height())) * image_.height(),
                  box.width() * image_.width(), box.height() * image_.height());
}

QPointF AmbiCanvas::viewportCenter() const
{
    if (QScrollArea* area = scrollArea()) {
        return QPointF(area->horizontalScrollBar()->value() + area->viewport()->width() / 2.0,
                       area->verticalScrollBar()->value() + area->viewport()->height() / 2.0);
    }
    return QPointF(width() / 2.0, height() / 2.0);
}

QPointF AmbiCanvas::toNormalized(const QPointF& point) const
{
    const QRectF rect = imageRect();
    if (rect.width() <= 0.0 || rect.height() <= 0.0) return QPointF(0.0, 0.0);
    return QPointF((point.x() - rect.left()) / rect.width(),
                   (point.y() - rect.top()) / rect.height());
}

QPointF AmbiCanvas::toWidget(const QPointF& normalized) const
{
    const QRectF rect = imageRect();
    return QPointF(rect.left() + normalized.x() * rect.width(),
                   rect.top() + normalized.y() * rect.height());
}

void AmbiCanvas::requestGeometryUpdate()
{
    const QSize target = documentSize();
    if (size() == target && minimumSize() == target && maximumSize() == target) return;
    setFixedSize(target);
    clampScrollBars();
}

void AmbiCanvas::clampScrollBars()
{
    if (QScrollArea* area = scrollArea()) {
        area->horizontalScrollBar()->setValue(area->horizontalScrollBar()->value());
        area->verticalScrollBar()->setValue(area->verticalScrollBar()->value());
    }
}

void AmbiCanvas::centerScroll()
{
    QScrollArea* area = scrollArea();
    if (!area) return;
    const QPoint target((documentSize().width() - area->viewport()->width()) / 2,
                        (documentSize().height() - area->viewport()->height()) / 2);
    area->horizontalScrollBar()->setValue(target.x());
    area->verticalScrollBar()->setValue(target.y());
    clampScrollBars();
}

void AmbiCanvas::setImage(const QPixmap& pixmap)
{
    image_ = pixmap;
    imageCache_ = pixmap;
    fitPending_ = !pixmap.isNull();
    if (pixmap.isNull()) {
        setFixedSize(viewportSize());
        update();
        return;
    }
    zoomToFit();
    update();
}

void AmbiCanvas::clearImage()
{
    image_ = QPixmap();
    imageCache_ = QPixmap();
    regionRects_.clear();
    regionColors_.clear();
    combined_ = QColor();
    panning_ = false;
    dragStart_ = QPointF();
    setFixedSize(viewportSize());
    if (QScrollArea* area = scrollArea()) {
        area->horizontalScrollBar()->setValue(0);
        area->verticalScrollBar()->setValue(0);
    }
    update();
}

void AmbiCanvas::setRegionSamples(const QVector<QRect>& rects, const QVector<QColor>& colors,
                                  const QColor& combined)
{
    regionRects_ = rects;
    regionColors_ = colors;
    combined_ = combined;
    update();
}

void AmbiCanvas::setRegion(const QString& region)
{
    region_ = region;
}

void AmbiCanvas::setBandPct(int percent)
{
    bandPct_ = percent;
}

void AmbiCanvas::setCustomRect(const QRectF& rect)
{
    customRect_ = rect;
}

void AmbiCanvas::setZones(const QVector<CaptureZone>& zones)
{
    zones_ = zones;
    if (activeZone_ >= zones_.size()) activeZone_ = zones_.isEmpty() ? -1 : 0;
    updateSelectionSize();
    update();
}

void AmbiCanvas::setActiveZone(int index)
{
    if (index == activeZone_) return;
    activeZone_ = (index >= 0 && index < zones_.size()) ? index : -1;
    updateSelectionSize();
    update();
    Q_EMIT activeZoneChanged(activeZone_);
}

void AmbiCanvas::setLayerColors(const QVector<QColor>& colors)
{
    layerColors_ = colors;
    update();
}

void AmbiCanvas::setGlobalTuning(double boost, double smooth, bool autoBright)
{
    globalBoost_ = boost;
    globalSmooth_ = smooth;
    globalAutoBright_ = autoBright;
    update();
}

void AmbiCanvas::setTool(Tool tool)
{
    tool_ = tool;
    // Курсор Tool подсказывает, что будет делать клик.
    switch (tool_) {
    case Tool::Move: setCursor(Qt::OpenHandCursor); break;
    case Tool::Rectangle: setCursor(Qt::CrossCursor); break;
    case Tool::Zoom: setCursor(Qt::CrossCursor); break;
    case Tool::Hand: setCursor(Qt::OpenHandCursor); break;
    }
}

void AmbiCanvas::setDrawMode(DrawMode mode)
{
    drawMode_ = mode;
}

void AmbiCanvas::setGridVisible(bool visible)
{
    gridVisible_ = visible;
    update();
}

void AmbiCanvas::setZoom(double zoom)
{
    zoom_ = std::clamp(zoom, 0.05, 16.0);
    requestGeometryUpdate();
    clampScrollBars();
    Q_EMIT viewChanged();
    update();
}

void AmbiCanvas::zoomBy(double factor)
{
    setZoomAndFit(zoom_ * factor, viewportCenter());
}

void AmbiCanvas::zoomToFit()
{
    if (image_.isNull()) {
        requestGeometryUpdate();
        return;
    }
    const QSize view = viewportSize();
    if (view.width() <= 2 * kMargin || view.height() <= 2 * kMargin) {
        fitPending_ = true;
        return;
    }
    const double scale = std::min((view.width() - 2.0 * kMargin) / image_.width(),
                                  (view.height() - 2.0 * kMargin) / image_.height());
    zoom_ = std::clamp(scale, 0.05, 16.0);
    requestGeometryUpdate();
    centerScroll();
    Q_EMIT viewChanged();
    update();
}

void AmbiCanvas::setZoomAndFit(double zoom, const QPointF& center)
{
    if (image_.isNull()) return;
    const QPointF anchor = toNormalized(center);
    setZoom(zoom);
    const QRectF after = imageRect();
    if (QScrollArea* area = scrollArea(); area && after.width() > 0.0) {
        const QPointF target(after.left() + anchor.x() * after.width() - center.x(),
                             after.top() + anchor.y() * after.height() - center.y());
        area->horizontalScrollBar()->setValue(qRound(target.x()));
        area->verticalScrollBar()->setValue(qRound(target.y()));
    }
    clampScrollBars();
    Q_EMIT viewChanged();
    update();
}

// Точка холста в координатах области просмотра.
QPointF AmbiCanvas::toViewport(const QPointF& canvasPoint) const
{
    QScrollArea* area = scrollArea();
    // Положение холста внутри viewport берём из геометрии, а не через mapTo:
    // mapTo обходит родителей и при временно нарушенном дереве (перенос холста
    // между вкладками) уходил в бесконечный обход — падение на ПКМ.
    if (!area || !area->widget()) return canvasPoint;
    return canvasPoint + QPointF(area->widget()->geometry().topLeft());
}

// Точный сдвиг 1:1 с накоплением дробной части: округление на каждом событии
// теряло медленные движения, а большой скачок уводил картинку рывком.
void AmbiCanvas::panBy(const QPointF& delta)
{
    panRemainder_ += delta;
    const double dx = std::trunc(panRemainder_.x());
    const double dy = std::trunc(panRemainder_.y());
    if (dx == 0.0 && dy == 0.0) return;
    panRemainder_ -= QPointF(dx, dy);
    // Знак минус: увеличение полосы прокрутки двигает картинку по экрану
    // влево, а захват в стиле Photoshop тянет её за курсором.
    scrollByPixels(-dx, -dy);
}

void AmbiCanvas::scrollByPixels(double dx, double dy)
{
    QScrollArea* area = scrollArea();
    if (!area) return;
    QScrollBar* horizontal = area->horizontalScrollBar();
    QScrollBar* vertical = area->verticalScrollBar();
    if (dx != 0.0) horizontal->setValue(horizontal->value() + qRound(dx));
    if (dy != 0.0) vertical->setValue(vertical->value() + qRound(dy));
}

// Захват холста мышью. Точка всегда в координатах области просмотра.
void AmbiCanvas::beginPan(const QPointF& cursorViewport)
{
    panning_ = true;
    panAnchorViewport_ = cursorViewport;
    panRemainder_ = QPointF();
    grabMouse();
    setCursor(Qt::ClosedHandCursor);
}

void AmbiCanvas::continuePan(const QPointF& cursorViewport)
{
    if (!panning_) return;
    panBy(cursorViewport - panAnchorViewport_);
    panAnchorViewport_ = cursorViewport;
}

void AmbiCanvas::endPan()
{
    if (!panning_) return;
    panning_ = false;
    releaseMouse();
    unsetCursor();
    if (tool_ == Tool::Hand) setCursor(Qt::OpenHandCursor);
}

void AmbiCanvas::drawGrid(QPainter& painter) const
{
    if (!gridVisible_) return;
    const QRectF rect = imageRect();
    painter.setPen(QPen(QColor(255, 255, 255, 40), 1));
    for (int i = 1; i < 3; ++i) {
        const double x = rect.left() + rect.width() * i / 3.0;
        const double y = rect.top() + rect.height() * i / 3.0;
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
}

QRectF AmbiCanvas::zoneBox(int index) const
{
    if (index < 0 || index >= zones_.size()) return QRectF();
    QRectF box;
    for (const QPolygonF& polygon : zones_.at(index).areas) {
        const QRectF r = polygonRect(polygon);
        if (r.isEmpty()) continue;
        box = box.isNull() ? r : box.united(r);
    }
    return box;
}

QPolygonF AmbiCanvas::rectToPolygon(const QRectF& rect) const
{
    return rectPolygon(rect);
}

int AmbiCanvas::zoneAt(const QPointF& normalized) const
{
    for (int i = zones_.size() - 1; i >= 0; --i) {
        for (const QPolygonF& polygon : zones_.at(i).areas) {
            if (polygon.containsPoint(normalized, Qt::OddEvenFill)) return i;
        }
    }
    return -1;
}

AmbiCanvas::Handle AmbiCanvas::handleAt(const QPointF& point) const
{
    if (activeZone_ < 0 || activeZone_ >= zones_.size()) return Handle::None;
    const QRectF box = zoneBox(activeZone_);
    if (box.isEmpty()) return Handle::None;
    const QRectF mapped(toWidget(box.topLeft()), toWidget(box.bottomRight()));
    const double left = mapped.left(), right = mapped.right();
    const double top = mapped.top(), bottom = mapped.bottom();
    const double cx = mapped.center().x(), cy = mapped.center().y();
    const auto near = [&](double x, double y) {
        return std::abs(point.x() - x) <= kGrabRadius && std::abs(point.y() - y) <= kGrabRadius;
    };
    if (near(left, top)) return Handle::TopLeft;
    if (near(cx, top)) return Handle::Top;
    if (near(right, top)) return Handle::TopRight;
    if (near(right, cy)) return Handle::Right;
    if (near(right, bottom)) return Handle::BottomRight;
    if (near(cx, bottom)) return Handle::Bottom;
    if (near(left, bottom)) return Handle::BottomLeft;
    if (near(left, cy)) return Handle::Left;
    return Handle::None;
}

void AmbiCanvas::beginScale(int index, Handle handle)
{
    if (index < 0 || index >= zones_.size()) return;
    dragOriginAreas_ = zones_.at(index).areas;
    dragOriginMasks_ = zones_.at(index).masks;
    dragOriginBox_ = zoneBox(index);
    dragHandle_ = handle;
    dragCurrent_ = QPointF();
}

void AmbiCanvas::applyScale(int index, Handle handle, const QPointF& normalized)
{
    if (index < 0 || index >= zones_.size()) return;
    if (dragOriginAreas_.isEmpty()) return;
    CaptureZone& zone = zones_[index];
    const QRectF start = dragOriginBox_;
    if (start.isEmpty() || start.width() < 1e-6 || start.height() < 1e-6) return;

    double left = start.left(), right = start.right(), top = start.top(), bottom = start.bottom();
    const double nx = std::clamp(normalized.x(), 0.0, 1.0);
    const double ny = std::clamp(normalized.y(), 0.0, 1.0);
    switch (handle) {
    case Handle::TopLeft: left = nx; top = ny; break;
    case Handle::Top: top = ny; break;
    case Handle::TopRight: right = nx; top = ny; break;
    case Handle::Right: right = nx; break;
    case Handle::BottomRight: right = nx; bottom = ny; break;
    case Handle::Bottom: bottom = ny; break;
    case Handle::BottomLeft: left = nx; bottom = ny; break;
    case Handle::Left: left = nx; break;
    default: return;
    }
    // Минимальный размер задаём рамкой, а не каждой точкой: иначе точки «прилипают»
    // к границам кадра и слой перестаёт нормально сжиматься.
    if (right - left < kMinZoneSize) {
        if (handle == Handle::Left || handle == Handle::TopLeft || handle == Handle::BottomLeft)
            left = right - kMinZoneSize;
        else
            right = left + kMinZoneSize;
    }
    if (bottom - top < kMinZoneSize) {
        if (handle == Handle::Top || handle == Handle::TopLeft || handle == Handle::TopRight)
            top = bottom - kMinZoneSize;
        else
            bottom = top + kMinZoneSize;
    }
    const double scaleX = (right - left) / start.width();
    const double scaleY = (bottom - top) / start.height();
    // Опорная точка — противоположная сторона, она остаётся на месте.
    const double baseX = (handle == Handle::TopLeft || handle == Handle::Left
                          || handle == Handle::BottomLeft) ? start.right() : start.left();
    const double baseY = (handle == Handle::TopLeft || handle == Handle::Top
                          || handle == Handle::TopRight) ? start.bottom() : start.top();

    const auto mapPoint = [&](const QPointF& point) {
        return QPointF(std::clamp(baseX + (point.x() - baseX) * scaleX, 0.0, 1.0),
                       std::clamp(baseY + (point.y() - baseY) * scaleY, 0.0, 1.0));
    };
    // Каждый вызов пересобирает геометрию из снимка, поэтому результат зависит
    // только от позиции курсора, а не от предыдущих кадров перетаскивания.
    const auto rebuild = [](QVector<QPolygonF>& target, const QVector<QPolygonF>& origin,
                            const std::function<QPointF(const QPointF&)>& mapper) {
        target.clear();
        target.reserve(origin.size());
        for (const QPolygonF& polygon : origin) {
            QPolygonF mapped;
            mapped.reserve(polygon.size());
            for (const QPointF& point : polygon) mapped.append(mapper(point));
            target.append(mapped);
        }
    };
    rebuild(zone.areas, dragOriginAreas_, mapPoint);
    rebuild(zone.masks, dragOriginMasks_, mapPoint);
    update();
    updateSelectionSize();
}

void AmbiCanvas::moveActiveZone(const QPointF& delta)
{
    if (activeZone_ < 0 || activeZone_ >= zones_.size()) return;
    if (activeZone_ < 0 || activeZone_ >= zones_.size()) return;
    CaptureZone& zone = zones_[activeZone_];
    const auto shift = [delta](QVector<QPolygonF>& polygons) {
        for (QPolygonF& polygon : polygons) {
            for (QPointF& point : polygon) {
                point = QPointF(std::clamp(point.x() + delta.x(), 0.0, 1.0),
                                std::clamp(point.y() + delta.y(), 0.0, 1.0));
            }
        }
    };
    shift(zone.areas);
    shift(zone.masks);
    update();
    updateSelectionSize();
}

void AmbiCanvas::updateSelectionSize()
{
    const QRectF pixels = selectionPixels();
    if (pixels.isEmpty()) {
        Q_EMIT selectionSizeChanged(0, 0);
        return;
    }
    Q_EMIT selectionSizeChanged(qRound(pixels.width()), qRound(pixels.height()));
}

void AmbiCanvas::commitPendingSelection(const QRectF& normalizedRect)
{
    QRectF rect = normalizedRect.normalized();
    if (rect.width() < kMinZoneSize || rect.height() < kMinZoneSize) return;
    rect = rect.intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (rect.width() < 1e-4 || rect.height() < 1e-4) return;

    if (drawMode_ == DrawMode::New) {
        // Новый прямоугольник заменяет выделение: если активного слоя нет,
        // создаём его, иначе переписываем его области.
        if (activeZone_ < 0 || activeZone_ >= zones_.size()) {
            CaptureZone zone;
            zone.name = tr("Layer %1").arg(zones_.size() + 1);
            zones_.append(zone);
            activeZone_ = zones_.size() - 1;
        }
        zones_[activeZone_].areas = {rectToPolygon(rect)};
        zones_[activeZone_].masks.clear();
    } else if (activeZone_ >= 0 && activeZone_ < zones_.size()) {
        CaptureZone& zone = zones_[activeZone_];
        if (drawMode_ == DrawMode::Add) {
            zone.areas.append(rectToPolygon(rect));
        } else {
            zone.masks.append(rectToPolygon(rect));
        }
    }
    pendingActive_ = false;
    updateSelectionSize();
    update();
    Q_EMIT zonesChanged();
    Q_EMIT activeZoneChanged(activeZone_);
}

void AmbiCanvas::drawZones(QPainter& painter) const
{
    for (int i = 0; i < zones_.size(); ++i) {
        const CaptureZone& zone = zones_.at(i);
        const QColor base = layerColorFor(layerColors_, i);
        for (const QPolygonF& polygon : zone.areas) {
            QPolygonF mapped;
            mapped.reserve(polygon.size());
            for (const QPointF& point : polygon) mapped.append(toWidget(point));
            QColor fill = base;
            fill.setAlpha(zone.enabled ? 46 : 16);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawPolygon(mapped);
            painter.setBrush(Qt::NoBrush);
            const bool active = i == activeZone_;
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(0, 0, 0, 130), 3, Qt::DashLine));
            painter.drawPolygon(mapped);
            painter.setPen(QPen(active ? kActiveBorder : kLayerBorder, active ? 2 : 1, Qt::DashLine));
            painter.drawPolygon(mapped);
        }
        for (const QPolygonF& polygon : zone.masks) {
            QPolygonF mapped;
            mapped.reserve(polygon.size());
            for (const QPointF& point : polygon) mapped.append(toWidget(point));
            painter.setPen(QPen(QColor(239, 68, 68), 2, Qt::DashLine));
            painter.setBrush(QBrush(QColor(239, 68, 68, 60), Qt::BDiagPattern));
            if (mapped.size() >= 3) {
                painter.drawPolygon(mapped);
            } else if (mapped.size() == 2) {
                painter.drawRect(polygonRect(mapped));
            }
        }
    }
    // Маркеры изменения размера активного слоя — как в Photoshop.
    if (activeZone_ >= 0 && activeZone_ < zones_.size() && tool_ == Tool::Move) {
        const QRectF box = zoneBox(activeZone_);
        if (!box.isEmpty()) {
            const QRectF mapped = QRectF(toWidget(box.topLeft()), toWidget(box.bottomRight()));
            const double h = kHandleSize / 2.0;
            const QPointF points[]{
                mapped.topLeft(), QPointF(mapped.center().x(), mapped.top()), mapped.topRight(),
                QPointF(mapped.right(), mapped.center().y()), mapped.bottomRight(),
                QPointF(mapped.center().x(), mapped.bottom()), mapped.bottomLeft(),
                QPointF(mapped.left(), mapped.center().y())};
            painter.setPen(QPen(QColor(255, 255, 255, 230), 1));
            painter.setBrush(kActiveBorder);
            for (const QPointF& point : points)
                painter.drawRect(QRectF(point.x() - h, point.y() - h, kHandleSize, kHandleSize));
        }
    }
}

void AmbiCanvas::drawPendingSelection(QPainter& painter) const
{
    if (!pendingActive_) return;
    const QRectF mapped = QRectF(toWidget(pendingRect_.topLeft()),
                                 toWidget(pendingRect_.bottomRight()));
    QColor line = drawMode_ == DrawMode::Subtract ? QColor(239, 68, 68) : kActiveBorder;
    QColor fill = line;
    fill.setAlpha(50);
    painter.setBrush(fill);
    painter.setPen(QPen(line, 2, Qt::DashLine));
    painter.drawRect(mapped);
}

void AmbiCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(24, 24, 30));
    if (image_.isNull()) {
        painter.setPen(QColor(140, 140, 155));
        painter.drawText(rect(), Qt::AlignCenter, tr("No image - capture the screen or load a file"));
        return;
    }

    const QRectF rect = imageRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(rect.toRect(), imageCache_);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(rect);

    // В простом режиме показываем готовую область захвата, в расширенном — слои.
    if (zones_.isEmpty()) {
        // buildRects() отдаёт прямоугольники в пикселях снимка, поэтому пересчитываем
        // их в координаты документа через размер картинки.
        const double scaleX = image_.width() > 0 ? rect.width() / image_.width() : 1.0;
        const double scaleY = image_.height() > 0 ? rect.height() / image_.height() : 1.0;
        for (int i = 0; i < regionRects_.size(); ++i) {
            const QRectF r(regionRects_.at(i));
            const QRectF mapped(rect.left() + r.x() * scaleX, rect.top() + r.y() * scaleY,
                                r.width() * scaleX, r.height() * scaleY);
            if (mapped.width() < 1.0 || mapped.height() < 1.0) continue;
            // Только фиолетовый пунктир: заливки нет, кадр остаётся настоящим.
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(0, 0, 0, 130), 3, Qt::DashLine));
            painter.drawRect(mapped);
            painter.setPen(QPen(kActiveBorder, 1, Qt::DashLine));
            painter.drawRect(mapped);
        }
    } else {
        drawZones(painter);
        drawPendingSelection(painter);
    }

    drawGrid(painter);
    painter.setClipping(false);

    if (combined_.isValid()) {
        // Образец итогового цвета в правом верхнем углу — как в исходной версии.
        const QRectF swatch(width() - 52, 12, 40, 40);
        painter.setPen(QPen(QColor(235, 235, 240), 2));
        painter.setBrush(combined_);
        painter.drawRoundedRect(swatch, 6, 6);
        painter.setPen(QPen(kActiveBorder, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(swatch.adjusted(1, 1, -1, -1), 5, 5);
    }
}

void AmbiCanvas::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (fitPending_) {
        fitPending_ = false;
        zoomToFit();
        return;
    }
    requestGeometryUpdate();
    clampScrollBars();
}

void AmbiCanvas::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    if (image_.isNull()) return;
    const QPointF point = event->position();
    const QPointF normalized = toNormalized(point);
    const bool inside = normalized.x() >= 0.0 && normalized.x() <= 1.0
                        && normalized.y() >= 0.0 && normalized.y() <= 1.0;

    // Захват холста: ПКМ, СКМ, пробел или инструмент «Рука». Считаем в
    // координатах области просмотра, иначе сдвиг замыкается сам на себя.
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton
        || spaceHeld_ || tool_ == Tool::Hand) {
        beginPan(toViewport(point));
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    if (tool_ == Tool::Zoom) {
        // Alt+клик — отдалить, как в Photoshop.
        zoomBy(event->modifiers().testFlag(Qt::AltModifier) ? 1.0 / 1.25 : 1.25);
        event->accept();
        return;
    }
    if (!inside) {
        event->accept();
        return;
    }
    if (tool_ == Tool::Rectangle) {
        pendingActive_ = true;
        pendingAnchor_ = normalized;
        pendingRect_ = QRectF(normalized, normalized);
        dragStart_ = point;
        update();
        event->accept();
        return;
    }
    // Инструмент перемещения: сначала маркер у активного слоя, иначе выбираем слой
    // под курсором и двигаем его.
    if (activeZone_ < 0 || activeZone_ >= zones_.size()) {
        setActiveZone(zoneAt(normalized));
    }
    const Handle handle = handleAt(point);
    if (handle != Handle::None) {
        beginScale(activeZone_, handle);
        dragStart_ = point;
        dragCurrent_ = normalized;
        event->accept();
        return;
    }
    const int index = zoneAt(normalized);
    if (index >= 0) {
        if (index != activeZone_) setActiveZone(index);
        beginScale(activeZone_, Handle::Move);
        dragStart_ = point;
        dragCurrent_ = normalized;
        event->accept();
    }
}

void AmbiCanvas::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF point = event->position();
    if (panning_) {
        continuePan(toViewport(point));
        event->accept();
        return;
    }
    if (pendingActive_) {
        const QPointF normalized = toNormalized(point);
        QRectF rect(pendingAnchor_, normalized);
        // Shift — квадрат, Alt — от центра. Это привычное поведение Photoshop.
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            const double side = std::max(std::abs(normalized.x() - pendingAnchor_.x()),
                                         std::abs(normalized.y() - pendingAnchor_.y()));
            const QPointF corner(pendingAnchor_.x() + (normalized.x() >= pendingAnchor_.x()
                                                            ? side : -side),
                                 pendingAnchor_.y() + (normalized.y() >= pendingAnchor_.y()
                                                            ? side : -side));
            rect = QRectF(pendingAnchor_, corner);
        } else if (event->modifiers().testFlag(Qt::AltModifier)) {
            const QPointF delta = normalized - pendingAnchor_;
            rect = QRectF(pendingAnchor_ - delta, pendingAnchor_ + delta);
        }
        pendingRect_ = rect;
        const QRectF pixels = selectionPixels();
        Q_UNUSED(pixels)
        const QRectF image = imageRect();
        if (image.width() > 0.0 && image.height() > 0.0) {
            const QRectF shown((rect.x() * image.width()), (rect.y() * image.height()),
                               rect.width() * image.width(), rect.height() * image.height());
            Q_EMIT selectionSizeChanged(qRound(shown.width()), qRound(shown.height()));
        }
        update();
        event->accept();
        return;
    }
    if (!dragOriginAreas_.isEmpty() && activeZone_ >= 0) {
        // Маркер берём из момента нажатия, а не из текущей геометрии слоя.
        const Handle handle = dragHandle_;
        if (handle == Handle::Move || handle == Handle::None) {
            const QPointF delta = toNormalized(point) - dragCurrent_;
            if (!delta.isNull()) {
                moveActiveZone(delta);
                dragCurrent_ = toNormalized(point);
            }
        } else {
            applyScale(activeZone_, handle, toNormalized(point));
        }
        event->accept();
        return;
    }
    if (!image_.isNull()) {
        const QPointF normalized = toNormalized(point);
        if (normalized.x() >= 0.0 && normalized.x() <= 1.0
            && normalized.y() >= 0.0 && normalized.y() <= 1.0) {
            Q_EMIT cursorMoved(qRound(normalized.x() * image_.width()),
                               qRound(normalized.y() * image_.height()));
        }
    }
    QWidget::mouseMoveEvent(event);
}

void AmbiCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (panning_) {
        endPan();
        event->accept();
        return;
    }
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    if (pendingActive_) {
        const QPointF normalized = toNormalized(event->position());
        QRectF rect(pendingAnchor_, normalized);
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            const double side = std::max(std::abs(normalized.x() - pendingAnchor_.x()),
                                         std::abs(normalized.y() - pendingAnchor_.y()));
            const QPointF corner(pendingAnchor_.x() + (normalized.x() >= pendingAnchor_.x()
                                                            ? side : -side),
                                 pendingAnchor_.y() + (normalized.y() >= pendingAnchor_.y()
                                                            ? side : -side));
            rect = QRectF(pendingAnchor_, corner);
        } else if (event->modifiers().testFlag(Qt::AltModifier)) {
            const QPointF delta = normalized - pendingAnchor_;
            rect = QRectF(pendingAnchor_ - delta, pendingAnchor_ + delta);
        }
        pendingRect_ = rect;
        commitPendingSelection(rect);
        pendingActive_ = false;
        event->accept();
        return;
    }
    if (!dragOriginAreas_.isEmpty()) {
        // Снимок больше не нужен: освобождаем память, следующий drag сделает новый.
        dragOriginAreas_.clear();
        dragOriginMasks_.clear();
        dragOriginBox_ = QRectF();
        dragHandle_ = Handle::None;
        Q_EMIT zonesChanged();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void AmbiCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !spaceHeld_) {
        spaceHeld_ = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void AmbiCanvas::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        spaceHeld_ = false;
        setTool(tool_);
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void AmbiCanvas::wheelEvent(QWheelEvent* event)
{
    if (image_.isNull()) return;
    const double steps = event->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(steps)) return;
    setZoomAndFit(zoom_ * std::pow(1.15, steps), event->position());
    event->accept();
}

bool AmbiCanvas::eventFilter(QObject* watched, QEvent* event)
{
    QScrollArea* area = scrollArea();
    if (!area || watched != area->viewport()) return QWidget::eventFilter(watched, event);

    // Документ фиксированного размера центрируется, поэтому поля вокруг картинки
    // принадлежат viewport. Пересчитываем координаты и отправляем событие себе.
    const QPoint origin = mapTo(area->viewport(), QPoint(0, 0));
    const auto toCanvas = [origin](const QPointF& point) { return point - QPointF(origin); };

    switch (event->type()) {
    case QEvent::Wheel: {
        auto* wheel = static_cast<QWheelEvent*>(event);
        const double steps = wheel->angleDelta().y() / 120.0;
        if (!image_.isNull() && !qFuzzyIsNull(steps)) {
            const QPointF position = toCanvas(wheel->position());
            const bool overImage = imageRect().contains(position);
            setZoomAndFit(zoom_ * std::pow(1.15, steps), overImage ? position : viewportCenter());
            wheel->accept();
            return true;
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const bool panButton = mouse->button() == Qt::MiddleButton
                               || mouse->button() == Qt::RightButton;
        const bool handDrag = mouse->button() == Qt::LeftButton
                              && (tool_ == Tool::Hand || spaceHeld_);
        if (!panButton && !handDrag) break;
        beginPan(mouse->position());
        mouse->accept();
        return true;
    }
    case QEvent::MouseMove: {
        if (!panning_) break;
        auto* mouse = static_cast<QMouseEvent*>(event);
        continuePan(mouse->position());
        mouse->accept();
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (!panning_) break;
        if (mouse->button() != Qt::MiddleButton && mouse->button() != Qt::RightButton
            && mouse->button() != Qt::LeftButton) {
            break;
        }
        endPan();
        mouse->accept();
        return true;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace elkbledom
