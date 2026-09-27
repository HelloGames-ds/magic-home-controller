#pragma once

#include <QColor>
#include <QPointF>
#include <QVector>
#include <QWidget>

namespace elkbledom {

// Поле кривой в духе Photoshop: сетка, гистограмма, перетаскиваемые точки.
// Работает с одним каналом; переключение каналов делает панель снаружи.
class CurveEditor : public QWidget
{
    Q_OBJECT
public:
    explicit CurveEditor(QWidget* parent = nullptr);

    // Точки в координатах 0..1 (x — вход, y — выход).
    QVector<QPointF> points() const { return points_; }
    void setPoints(const QVector<QPointF>& points);
    // Экранные координаты точки кривой: нужно, чтобы панель и тесты знали, где
    // на самом деле лежит точка, а не гадали по координатам кривой.
    QPointF toWidgetPublic(const QPointF& curvePoint) const { return toWidget(curvePoint); }
    void setHistogram(const QVector<int>& red, const QVector<int>& green,
                      const QVector<int>& blue);
    void setChannelColor(const QColor& color);
    // Какой канал показывать: 0 — общий RGB, 1 — красный, 2 — зелёный, 3 — синий.
    // Раньше гистограмма всегда брала красный канал, и переключение ничего
    // не меняло.
    void setChannel(int channel);
    int channel() const { return channel_; }
    void resetToLinear();

    QSize sizeHint() const override { return QSize(240, 240); }
    QSize minimumSizeHint() const override { return QSize(160, 160); }

Q_SIGNALS:
    void curvesChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPointF toCurve(const QPointF& widgetPoint) const;
    QPointF toWidget(const QPointF& curvePoint) const;
    int pointAt(const QPointF& curvePoint) const;
    int indexOfPointNear(const QPointF& curvePoint) const;
    void insertSorted(QPointF curvePoint);
    void drawGrid(QPainter& painter) const;
    void drawHistogram(QPainter& painter) const;
    void drawCurve(QPainter& painter) const;

    QVector<QPointF> points_;
    QVector<int> histogramR_;
    QVector<int> histogramG_;
    QVector<int> histogramB_;
    QColor channelColor_{ QColor(200, 200, 210) };
    int channel_ = 0;
    int dragging_ = -1;
};

} // namespace elkbledom
