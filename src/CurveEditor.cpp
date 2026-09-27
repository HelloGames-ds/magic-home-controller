#include "CurveEditor.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace elkbledom {
namespace {

constexpr int kMargin = 10;
constexpr int kGrabRadius = 9;

} // namespace

CurveEditor::CurveEditor(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(160, 160);
    setMouseTracking(false);
    setFocusPolicy(Qt::StrongFocus);
    setPoints({ QPointF(0.0, 0.0), QPointF(1.0, 1.0) });
}

void CurveEditor::setPoints(const QVector<QPointF>& points)
{
    points_ = points;
    if (points_.size() < 2) points_ = { QPointF(0.0, 0.0), QPointF(1.0, 1.0) };
    std::stable_sort(points_.begin(), points_.end(),
                     [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });
    update();
}

void CurveEditor::setHistogram(const QVector<int>& red, const QVector<int>& green,
                               const QVector<int>& blue)
{
    histogramR_ = red;
    histogramG_ = green;
    histogramB_ = blue;
    update();
}

void CurveEditor::setChannelColor(const QColor& color)
{
    channelColor_ = color.isValid() ? color : QColor(200, 200, 210);
    update();
}

void CurveEditor::resetToLinear()
{
    setPoints({ QPointF(0.0, 0.0), QPointF(1.0, 1.0) });
    Q_EMIT curvesChanged();
}

QPointF CurveEditor::toCurve(const QPointF& widgetPoint) const
{
    const double w = std::max(1, width() - 2 * kMargin);
    const double h = std::max(1, height() - 2 * kMargin);
    const double x = std::clamp((widgetPoint.x() - kMargin) / w, 0.0, 1.0);
    const double y = std::clamp(1.0 - (widgetPoint.y() - kMargin) / h, 0.0, 1.0);
    return QPointF(x, y);
}

QPointF CurveEditor::toWidget(const QPointF& curvePoint) const
{
    const double w = std::max(1, width() - 2 * kMargin);
    const double h = std::max(1, height() - 2 * kMargin);
    return QPointF(kMargin + curvePoint.x() * w, kMargin + (1.0 - curvePoint.y()) * h);
}

int CurveEditor::pointAt(const QPointF& curvePoint) const
{
    int best = -1;
    double bestDistance = kGrabRadius;
    for (int i = 0; i < points_.size(); ++i) {
        const double dx = toWidget(points_.at(i)).x() - toWidget(curvePoint).x();
        const double dy = toWidget(points_.at(i)).y() - toWidget(curvePoint).y();
        const double distance = std::hypot(dx, dy);
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

void CurveEditor::insertSorted(QPointF curvePoint)
{
    points_.append(curvePoint);
    std::stable_sort(points_.begin(), points_.end(),
                     [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });
}

void CurveEditor::drawGrid(QPainter& painter) const
{
    painter.fillRect(rect(), QColor(30, 30, 36));
    painter.setPen(QPen(QColor(52, 52, 62), 1.0));
    for (int i = 0; i <= 4; ++i) {
        const double t = i / 4.0;
        const QPointF a = toWidget(QPointF(t, 0.0));
        const QPointF b = toWidget(QPointF(t, 1.0));
        painter.drawLine(QPointF(a.x(), kMargin), QPointF(b.x(), height() - kMargin));
        const QPointF c = toWidget(QPointF(0.0, t));
        const QPointF d = toWidget(QPointF(1.0, t));
        painter.drawLine(QPointF(kMargin, c.y()), QPointF(width() - kMargin, d.y()));
    }
    // Диагональ без правки — опора, как в Photoshop.
    painter.setPen(QPen(QColor(70, 70, 84), 1.0, Qt::DashLine));
    painter.drawLine(toWidget(QPointF(0.0, 0.0)), toWidget(QPointF(1.0, 1.0)));
}

void CurveEditor::setChannel(int channel)
{
    channel_ = std::clamp(channel, 0, 3);
    update();
}

void CurveEditor::drawHistogram(QPainter& painter) const
{
    // Каналы берём по выбранному: иначе подпись канала менялась, а данные нет.
    QVector<int> merged(256, 0);
    const QVector<int>* source = nullptr;
    switch (channel_) {
    case 1: source = &histogramR_; break;
    case 2: source = &histogramG_; break;
    case 3: source = &histogramB_; break;
    default:
        // Общий канал — сумма трёх, как в Photoshop.
        for (int i = 0; i < 256; ++i)
            merged[i] = (i < histogramR_.size() ? histogramR_.at(i) : 0)
                        + (i < histogramG_.size() ? histogramG_.at(i) : 0)
                        + (i < histogramB_.size() ? histogramB_.at(i) : 0);
        source = merged.isEmpty() ? nullptr : &merged;
        break;
    }
    if (!source || source->isEmpty()) return;
    int peak = 0;
    for (int value : *source) peak = std::max(peak, value);
    if (peak <= 0) return;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(channelColor_.red(), channelColor_.green(), channelColor_.blue(), 70));
    const int w = std::max(1, width() - 2 * kMargin);
    for (int i = 0; i < 256; ++i) {
        const double share = double(source->at(i)) / double(peak);
        const double barHeight = share * (height() - 2 * kMargin) * 0.9;
        const int x = kMargin + int(i / 255.0 * w);
        painter.drawRect(QRect(x, height() - kMargin - int(barHeight), 1, int(barHeight) + 1));
    }
}

void CurveEditor::drawCurve(QPainter& painter) const
{
    QPainterPath path;
    bool started = false;
    // Кривую рисуем по всей ширине поля с шагом в пиксель, поэтому она гладкая.
    for (int px = 0; px <= width(); ++px) {
        const QPointF input = toCurve(QPointF(px, kMargin));
        double output = input.y();
        for (int i = 0; i + 1 < points_.size(); ++i) {
            const QPointF& a = points_.at(i);
            const QPointF& b = points_.at(i + 1);
            if (input.x() < a.x() || input.x() > b.x()) continue;
            const double span = b.x() - a.x();
            const double t = span > 1e-9 ? (input.x() - a.x()) / span : 0.0;
            output = a.y() + (b.y() - a.y()) * t;
            break;
        }
        const QPointF at(px, toWidget(QPointF(input.x(), output)).y());
        if (!started) {
            path.moveTo(at);
            started = true;
        } else {
            path.lineTo(at);
        }
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(channelColor_, 1.6));
    painter.drawPath(path);

    painter.setBrush(QColor(20, 20, 26));
    painter.setPen(QPen(channelColor_, 1.4));
    for (const QPointF& point : points_) {
        const QPointF at = toWidget(point);
        painter.drawEllipse(at, 4.0, 4.0);
    }
}

void CurveEditor::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    drawGrid(painter);
    drawHistogram(painter);
    drawCurve(painter);
}

void CurveEditor::mousePressEvent(QMouseEvent* event)
{
    const QPointF at = toCurve(event->position());
    const int index = pointAt(at);
    if (event->button() == Qt::RightButton) {
        // Крайние точки не удаляем: без них кривая не определена.
        if (index > 0 && index < points_.size() - 1) {
            points_.remove(index);
            update();
            Q_EMIT curvesChanged();
        }
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    if (index >= 0) {
        dragging_ = index;
        return;
    }
    insertSorted(at);
    dragging_ = indexOfPointNear(at);
    update();
    Q_EMIT curvesChanged();
}

int CurveEditor::indexOfPointNear(const QPointF& curvePoint) const
{
    int best = 0;
    double bestDistance = 1e9;
    for (int i = 0; i < points_.size(); ++i) {
        const double distance = std::abs(points_.at(i).x() - curvePoint.x());
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

void CurveEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_ < 0 || dragging_ >= points_.size()) return;
    QPointF at = toCurve(event->position());
    at.setX(std::clamp(at.x(), 0.0, 1.0));
    at.setY(std::clamp(at.y(), 0.0, 1.0));
    // Точки не должны перепрыгивать друг друга: иначе кривая «переворачивается».
    if (dragging_ > 0) at.setX(std::max(at.x(), points_.at(dragging_ - 1).x()));
    if (dragging_ + 1 < points_.size())
        at.setX(std::min(at.x(), points_.at(dragging_ + 1).x()));
    points_[dragging_] = at;
    update();
    Q_EMIT curvesChanged();
}

void CurveEditor::mouseReleaseEvent(QMouseEvent*)
{
    dragging_ = -1;
}

} // namespace elkbledom
