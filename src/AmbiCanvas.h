#pragma once

#include <QColor>
#include <QPointF>
#include <QPixmap>
#include <QPolygonF>
#include <QRectF>
#include <QVector>
#include <QWidget>

#include "CaptureZone.h"

QT_BEGIN_NAMESPACE
class QKeyEvent;
class QScrollArea;
class QScrollBar;
class QWheelEvent;
QT_END_NAMESPACE

namespace elkbledom {

// Холст предпросмотра. В простом режиме показывает снимок экрана и подсвечивает
// готовую область захвата. В расширенном режиме дополнительно рисует слои и
// позволяет создавать их прямоугольником — как в Photoshop: Shift держит квадрат,
// Alt рисует от центра, режимы New/Add/Subtract меняют что происходит с выделением.
class AmbiCanvas final : public QWidget
{
    Q_OBJECT

public:
    enum class Tool { Move, Rectangle, Zoom, Hand };
    // New — заменить выделение слоя, Add — добавить область, Subtract — вычесть.
    enum class DrawMode { New, Add, Subtract };

    explicit AmbiCanvas(QWidget* parent = nullptr);

    void setImage(const QPixmap& pixmap);
    void clearImage();
    bool hasImage() const { return !image_.isNull(); }

    // Область захвата из готового пресета (простой режим).
    void setRegionSamples(const QVector<QRect>& rects, const QVector<QColor>& colors,
                          const QColor& combined);
    void setRegion(const QString& region);
    void setBandPct(int percent);
    void setCustomRect(const QRectF& rect);

    // Слои (расширенный режим).
    void setZones(const QVector<CaptureZone>& zones);
    const QVector<CaptureZone>& zones() const { return zones_; }
    void setActiveZone(int index);
    int activeZone() const { return activeZone_; }
    void setLayerColors(const QVector<QColor>& colors);
    // Общие настройки — показываются серым у слоёв, которые их не переопределяют.
    void setGlobalTuning(double boost, double smooth, bool autoBright);

    void setTool(Tool tool);
    Tool tool() const { return tool_; }
    void setDrawMode(DrawMode mode);
    DrawMode drawMode() const { return drawMode_; }

    void setGridVisible(bool visible);
    bool gridVisible() const { return gridVisible_; }

    // Масштаб и панорамирование.
    void setZoom(double zoom);
    double zoom() const { return zoom_; }
    void zoomToFit();
    void zoomBy(double factor);
    void setZoomAndFit(double zoom, const QPointF& center);
    void scrollByPixels(double dx, double dy);
    void panBy(const QPointF& delta);
    // Захват холста: точки передаются в координатах области просмотра.
    void beginPan(const QPointF& cursorViewport);
    void continuePan(const QPointF& cursorViewport);
    void endPan();
    // Точка холста в координатах области просмотра. Сдвиг при панорамировании
    // обязан считаться в них: сам холст двигается при прокрутке, и сдвиг в
    // его координатах даёт положительную обратную связь (картинка «убегает»).
    QPointF toViewport(const QPointF& canvasPoint) const;
    QPointF viewportCenter() const;

    QRectF imageRect() const;
    QSize documentSize() const;
    // Выделение активного слоя в пикселях снимка — для полей W/H в верхней панели.
    QRectF selectionPixels() const;

Q_SIGNALS:
    void cursorMoved(int x, int y);
    void viewChanged();
    // Слой создан или изменён: редактор пересчитывает историю и настройки.
    void zonesChanged();
    void activeZoneChanged(int index);
    void selectionSizeChanged(int width, int height);

protected:
    QSize sizeHint() const override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    // События полей вокруг картинки перенаправляются в canvas: иначе зум и
    // панорамирование работают только при наведении точно на изображение.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class Handle { None, Move, TopLeft, Top, TopRight, Right, BottomRight, Bottom,
                        BottomLeft, Left };

    QScrollArea* scrollArea() const;
    QSize viewportSize() const;
    QPointF toNormalized(const QPointF& point) const;
    QPointF toWidget(const QPointF& normalized) const;
    void requestGeometryUpdate();
    void clampScrollBars();
    void centerScroll();
    void drawGrid(QPainter& painter) const;
    void drawZones(QPainter& painter) const;
    void drawPendingSelection(QPainter& painter) const;

    int zoneAt(const QPointF& normalized) const;
    Handle handleAt(const QPointF& point) const;
    QRectF zoneBox(int index) const;
    QPolygonF rectToPolygon(const QRectF& rect) const;
    void beginScale(int index, Handle handle);
    void applyScale(int index, Handle handle, const QPointF& normalized);
    void moveActiveZone(const QPointF& delta);
    void commitPendingSelection(const QRectF& normalizedRect);
    void updateSelectionSize();

    QPixmap image_;
    QPixmap imageCache_;
    QString region_;
    QRectF customRect_;
    int bandPct_ = 8;

    QVector<QRect> regionRects_;
    QVector<QColor> regionColors_;
    QColor combined_;

    QVector<CaptureZone> zones_;
    QVector<QColor> layerColors_;
    int activeZone_ = -1;
    double globalBoost_ = 1.3;
    double globalSmooth_ = 0.3;
    bool globalAutoBright_ = true;

    Tool tool_ = Tool::Move;
    DrawMode drawMode_ = DrawMode::New;
    QRectF pendingRect_;      // нормализованный прямоугольник в процессе рисования
    bool pendingCut_ = false;
    bool pendingActive_ = false;
    QPointF pendingAnchor_;   // точка, от которой тянем (Shift/Alt меняют геометрию)

    bool gridVisible_ = false;

    double zoom_ = 1.0;
    bool fitPending_ = false;
    QPointF dragStart_;
    QPointF dragCurrent_;
    QPointF panRemainder_;
    QPointF panAnchorViewport_;
    bool panning_ = false;
    bool spaceHeld_ = false;
    // Снимок геометрии на момент захвата маркера: масштабирование всегда строится
    // заново из снимка, поэтому многократные mouseMove не накапливают искажение.
    // Маркер запоминаем отдельно: hitAt во время перетаскивания смотрит на уже
    // изменившуюся геометрию и вернул бы другой маркер.
    Handle dragHandle_ = Handle::None;
    QRectF dragOriginBox_;
    QVector<QPolygonF> dragOriginAreas_;
    QVector<QPolygonF> dragOriginMasks_;
};

} // namespace elkbledom
