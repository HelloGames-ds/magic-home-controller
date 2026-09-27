#include "EditorIcons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>

namespace elkbledom {
namespace icons {
namespace {

// Рисуем иконку в логических координатах 24x24, а затем масштабируем под запрос.
struct Pen {
    QColor color;
    qreal width = 1.8;
    Qt::PenCapStyle cap = Qt::RoundCap;
    Qt::PenJoinStyle join = Qt::RoundJoin;
    Qt::PenStyle style = Qt::SolidLine;
};

void stroke(QPainter& painter, const Pen& pen)
{
    QPen p(pen.color, pen.width, pen.style);
    p.setCapStyle(pen.cap);
    p.setJoinStyle(pen.join);
    painter.setPen(p);
    painter.setBrush(Qt::NoBrush);
}

void line(QPainter& painter, qreal x1, qreal y1, qreal x2, qreal y2)
{
    painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
}

void rect(QPainter& painter, const QRectF& r)
{
    painter.drawRect(r);
}

void poly(QPainter& painter, const QPolygonF& p)
{
    painter.drawPolygon(p);
}

void circle(QPainter& painter, qreal cx, qreal cy, qreal r)
{
    painter.drawEllipse(QPointF(cx, cy), r, r);
}

// Каждая иконка рисуется в системе координат 24x24 без transforms.
using PainterFn = void (*)(QPainter&);

void drawSelect(QPainter& p)
{
    QPainterPath path;
    path.moveTo(6, 3.5);
    path.lineTo(6, 16.5);
    path.lineTo(9.8, 13.2);
    path.lineTo(12.3, 18.8);
    path.lineTo(14.6, 17.6);
    path.lineTo(12.2, 12.2);
    path.lineTo(17, 11.6);
    path.closeSubpath();
    p.setBrush(p.pen().color());
    p.setPen(Qt::NoPen);
    p.drawPath(path);
}

void drawLasso(QPainter& p)
{
    QPainterPath path;
    path.moveTo(20.5, 11.2);
    path.cubicTo(20.5, 6.6, 16.2, 3.4, 11, 3.9);
    path.cubicTo(5.8, 4.4, 3.0, 7.6, 3.6, 11.4);
    path.cubicTo(4.1, 15.2, 7.4, 17.0, 11.0, 16.6);
    path.cubicTo(13.4, 16.3, 15.4, 15.2, 16.6, 13.6);
    p.drawPath(path);
    // Хвост «лассо» закручивается вниз и обратно.
    QPainterPath tail;
    tail.moveTo(16.6, 13.6);
    tail.cubicTo(14.6, 15.4, 14.0, 17.6, 15.6, 18.6);
    tail.cubicTo(17.0, 19.5, 18.0, 18.6, 17.4, 17.8);
    p.drawPath(tail);
    circle(p, 17.2, 17.4, 0.1);
}

void drawRectangle(QPainter& p)
{
    rect(p, QRectF(3.5, 5.0, 17.0, 14.0));
    // Штриховка внутри, как у инструмента выделения прямоугольником.
    for (qreal x = 6.0; x < 18.0; x += 3.0) line(p, x, 5.6, x - 3.0, 18.4);
}

void drawCutLasso(QPainter& p)
{
    // Лассо, как у вычитания, с минусом в круге.
    QPainterPath path;
    path.moveTo(19.6, 10.6);
    path.cubicTo(19.6, 6.6, 15.8, 3.8, 11.0, 4.2);
    path.cubicTo(6.2, 4.6, 3.6, 7.6, 4.1, 11.0);
    path.cubicTo(4.6, 14.4, 7.6, 16.4, 10.8, 16.0);
    p.drawPath(path);
    QPainterPath tail;
    tail.moveTo(10.8, 16.0);
    tail.cubicTo(9.2, 17.6, 9.0, 19.4, 10.4, 20.0);
    p.drawPath(tail);
    line(p, 13.0, 19.0, 20.5, 19.0);
}

void drawCutRectangle(QPainter& p)
{
    rect(p, QRectF(3.0, 3.5, 14.0, 14.0));
    line(p, 14.0, 15.0, 21.0, 22.0);
    line(p, 14.0, 15.0, 20.0, 15.0);
}

void drawPan(QPainter& p)
{
    QPainterPath path;
    path.moveTo(8.5, 12.5);
    path.lineTo(8.5, 6.0);
    path.cubicTo(8.5, 4.9, 10.1, 4.9, 10.1, 6.0);
    path.lineTo(10.1, 10.4);
    path.lineTo(10.1, 4.6);
    path.cubicTo(10.1, 3.5, 11.7, 3.5, 11.7, 4.6);
    path.lineTo(11.7, 10.4);
    path.lineTo(11.7, 5.2);
    path.cubicTo(11.7, 4.1, 13.3, 4.1, 13.3, 5.2);
    path.lineTo(13.3, 10.6);
    path.lineTo(13.3, 6.6);
    path.cubicTo(13.3, 5.5, 14.9, 5.5, 14.9, 6.6);
    path.lineTo(14.9, 14.2);
    path.cubicTo(14.9, 18.2, 12.4, 20.5, 9.6, 20.5);
    path.cubicTo(7.0, 20.5, 5.0, 18.6, 4.2, 16.4);
    path.cubicTo(3.7, 15.1, 4.5, 14.2, 5.4, 14.8);
    path.lineTo(8.5, 16.6);
    path.closeSubpath();
    p.drawPath(path);
}

void drawZoomIn(QPainter& p)
{
    circle(p, 10.5, 10.5, 6.2);
    line(p, 15.0, 15.0, 20.5, 20.5);
    line(p, 7.6, 10.5, 13.4, 10.5);
    line(p, 10.5, 7.6, 10.5, 13.4);
}

void drawZoomOut(QPainter& p)
{
    circle(p, 10.5, 10.5, 6.2);
    line(p, 15.0, 15.0, 20.5, 20.5);
    line(p, 7.6, 10.5, 13.4, 10.5);
}

void drawFit(QPainter& p)
{
    rect(p, QRectF(3.0, 5.0, 18.0, 14.0));
    // Стрелки внутрь.
    QPolygonF top{ QPointF(8.0, 12.0), QPointF(12.0, 8.0), QPointF(16.0, 12.0) };
    poly(p, top);
    QPolygonF bottom{ QPointF(8.0, 14.0), QPointF(12.0, 18.0), QPointF(16.0, 14.0) };
    poly(p, bottom);
}

void drawGrid(QPainter& p)
{
    rect(p, QRectF(3.5, 3.5, 17.0, 17.0));
    for (qreal v = 9.0; v < 20.0; v += 3.7) {
        line(p, v, 3.9, v, 20.1);
        line(p, 3.9, v, 20.1, v);
    }
}

void drawSplit(QPainter& p)
{
    rect(p, QRectF(3.0, 5.0, 18.0, 14.0));
    line(p, 12.0, 5.0, 12.0, 19.0);
    p.setBrush(p.pen().color());
    poly(p, QPolygonF{ QPointF(9.6, 12.0), QPointF(14.4, 9.4), QPointF(14.4, 14.6) });
}

void drawFullscreen(QPainter& p)
{
    QPolygonF corners{
        QPointF(3.5, 8.5), QPointF(3.5, 3.5), QPointF(8.5, 3.5),
        QPointF(15.5, 3.5), QPointF(20.5, 3.5), QPointF(20.5, 8.5),
        QPointF(20.5, 15.5), QPointF(20.5, 20.5), QPointF(15.5, 20.5),
        QPointF(8.5, 20.5), QPointF(3.5, 20.5), QPointF(3.5, 15.5) };
    poly(p, corners);
}

void drawCapture(QPainter& p)
{
    rect(p, QRectF(3.0, 6.0, 18.0, 14.0));
    QPolygonF hump{ QPointF(8.0, 6.0), QPointF(9.8, 3.5), QPointF(14.2, 3.5), QPointF(16.0, 6.0) };
    poly(p, hump);
    circle(p, 12.0, 13.0, 3.6);
}

void drawOpen(QPainter& p)
{
    QPainterPath path;
    path.moveTo(3.0, 19.0);
    path.lineTo(3.0, 5.0);
    path.lineTo(9.5, 5.0);
    path.lineTo(11.5, 7.5);
    path.lineTo(21.0, 7.5);
    path.lineTo(21.0, 19.0);
    path.closeSubpath();
    p.drawPath(path);
}

void drawTrash(QPainter& p)
{
    line(p, 3.5, 6.5, 20.5, 6.5);
    QPainterPath lid;
    lid.moveTo(9.0, 6.5);
    lid.lineTo(9.0, 4.0);
    lid.lineTo(15.0, 4.0);
    lid.lineTo(15.0, 6.5);
    p.drawPath(lid);
    QPainterPath can;
    can.moveTo(5.5, 6.5);
    can.lineTo(6.4, 20.0);
    can.lineTo(17.6, 20.0);
    can.lineTo(18.5, 6.5);
    p.drawPath(can);
    line(p, 10.0, 10.0, 10.4, 16.5);
    line(p, 14.0, 10.0, 13.6, 16.5);
}

void drawMirrorX(QPainter& p)
{
    line(p, 12.0, 3.0, 12.0, 21.0);
    QPainterPath left;
    left.moveTo(9.5, 8.0); left.lineTo(4.0, 12.0); left.lineTo(9.5, 16.0);
    p.drawPath(left);
    QPainterPath right;
    right.moveTo(14.5, 8.0); right.lineTo(20.0, 12.0); right.lineTo(14.5, 16.0);
    p.drawPath(right);
}

void drawMirrorY(QPainter& p)
{
    line(p, 3.0, 12.0, 21.0, 12.0);
    QPainterPath top;
    top.moveTo(8.0, 9.5); top.lineTo(12.0, 4.0); top.lineTo(16.0, 9.5);
    p.drawPath(top);
    QPainterPath bottom;
    bottom.moveTo(8.0, 14.5); bottom.lineTo(12.0, 20.0); bottom.lineTo(16.0, 14.5);
    p.drawPath(bottom);
}

void drawRotate(QPainter& p)
{
    QPainterPath arc;
    arc.moveTo(19.0, 8.0);
    arc.arcTo(QRectF(4.0, 4.0, 16.0, 16.0), 40.0, 280.0);
    p.drawPath(arc);
    QPolygonF head{ QPointF(19.0, 3.6), QPointF(21.6, 8.4), QPointF(16.2, 8.0) };
    p.setBrush(p.pen().color());
    p.setPen(Qt::NoPen);
    poly(p, head);
}

void drawPlus(QPainter& p)
{
    circle(p, 12.0, 12.0, 8.0);
    line(p, 12.0, 8.0, 12.0, 16.0);
    line(p, 8.0, 12.0, 16.0, 12.0);
}

void drawCamera(QPainter& p)
{
    rect(p, QRectF(2.5, 6.0, 13.0, 10.0));
    QPolygonF hump{ QPointF(5.5, 6.0), QPointF(7.0, 3.5), QPointF(11.0, 3.5), QPointF(12.5, 6.0) };
    poly(p, hump);
    QPainterPath frame;
    frame.moveTo(9.0, 11.0);
    frame.lineTo(9.0, 22.0);
    frame.lineTo(19.0, 22.0);
    frame.lineTo(19.0, 11.0);
    p.drawPath(frame);
    line(p, 6.0, 20.0, 22.0, 20.0);
}

void drawDuplicate(QPainter& p)
{
    rect(p, QRectF(4.0, 3.0, 11.5, 11.5));
    p.setBrush(p.pen().color());
    QPainterPath front;
    front.moveTo(9.0, 20.5);
    front.lineTo(9.0, 9.5);
    front.lineTo(20.0, 9.5);
    front.lineTo(20.0, 20.5);
    front.closeSubpath();
    p.setPen(Qt::NoPen);
    p.drawPath(front);
}

void drawArrowUp(QPainter& p)
{
    line(p, 12.0, 19.5, 12.0, 4.5);
    QPainterPath head;
    head.moveTo(6.5, 10.0);
    head.lineTo(12.0, 4.5);
    head.lineTo(17.5, 10.0);
    p.setBrush(p.pen().color());
    p.setPen(Qt::NoPen);
    p.drawPath(head);
}

void drawArrowDown(QPainter& p)
{
    line(p, 12.0, 4.5, 12.0, 19.5);
    QPainterPath head;
    head.moveTo(6.5, 14.0);
    head.lineTo(12.0, 19.5);
    head.lineTo(17.5, 14.0);
    p.setBrush(p.pen().color());
    p.setPen(Qt::NoPen);
    p.drawPath(head);
}

void drawUndo(QPainter& p)
{
    QPainterPath arc;
    arc.moveTo(4.0, 12.0);
    arc.arcTo(QRectF(4.5, 5.0, 15.0, 15.0), 180.0, -180.0);
    p.drawPath(arc);
    QPolygonF head{ QPointF(3.2, 8.0), QPointF(9.2, 12.0), QPointF(3.2, 16.0) };
    p.setBrush(p.pen().color());
    p.setPen(Qt::NoPen);
    p.drawPolygon(head);
}

void drawRedo(QPainter& p)
{
    QPainterPath arc;
    arc.moveTo(20.0, 12.0);
    arc.arcTo(QRectF(4.5, 5.0, 15.0, 15.0), 0.0, 180.0);
    p.drawPath(arc);
    QPolygonF head{ QPointF(20.8, 8.0), QPointF(14.8, 12.0), QPointF(20.8, 16.0) };
    p.setBrush(p.pen().color());
    p.setPen(Qt::NoPen);
    p.drawPolygon(head);
}

void drawPanelLeft(QPainter& p)
{
    rect(p, QRectF(3.0, 4.5, 18.0, 15.0));
    line(p, 9.5, 4.5, 9.5, 19.5);
    p.setBrush(p.pen().color());
    p.setPen(Qt::NoPen);
    p.drawRect(QRectF(3.6, 5.1, 5.3, 13.8));
}

void drawPanelRight(QPainter& p)
{
    rect(p, QRectF(3.0, 4.5, 18.0, 15.0));
    line(p, 14.5, 4.5, 14.5, 19.5);
    p.setBrush(p.pen().color());
    p.setPen(Qt::NoPen);
    p.drawRect(QRectF(15.1, 5.1, 5.3, 13.8));
}

PainterFn painterFor(Id id){
    switch (id) {
    case Id::Select: return drawSelect;
    case Id::Lasso: return drawLasso;
    case Id::Rectangle: return drawRectangle;
    case Id::CutLasso: return drawCutLasso;
    case Id::CutRectangle: return drawCutRectangle;
    case Id::Pan: return drawPan;
    case Id::ZoomIn: return drawZoomIn;
    case Id::ZoomOut: return drawZoomOut;
    case Id::Fit: return drawFit;
    case Id::Grid: return drawGrid;
    case Id::Split: return drawSplit;
    case Id::Fullscreen: return drawFullscreen;
    case Id::Capture: return drawCapture;
    case Id::Open: return drawOpen;
    case Id::Trash: return drawTrash;
    case Id::MirrorX: return drawMirrorX;
    case Id::MirrorY: return drawMirrorY;
    case Id::Rotate: return drawRotate;
    case Id::Plus: return drawPlus;
    case Id::Camera: return drawCamera;
    case Id::Duplicate: return drawDuplicate;
    case Id::ArrowUp: return drawArrowUp;
    case Id::ArrowDown: return drawArrowDown;
    case Id::Undo: return drawUndo;
    case Id::Redo: return drawRedo;
    case Id::PanelLeft: return drawPanelLeft;
    case Id::PanelRight: return drawPanelRight;
    }
    return drawRectangle;
}

QPixmap render(Id id, const QColor& color, int size, qreal devicePixelRatio)
{
    const int px = qMax(1, qRound(size * devicePixelRatio));
    QPixmap pixmap(px, px);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(size / 24.0, size / 24.0);

    // Обводка чуть тоньше на мелких размерах, чтобы иконка не «забивалась».
    Pen pen{ color, size <= 18 ? 1.9 : 1.7 };
    stroke(painter, pen);
    painterFor(id)(painter);
    painter.end();
    return pixmap;
}

} // namespace

QIcon make(Id id, const QColor& color, int size)
{
    const qreal ratio = 1.0;
    QIcon icon;
    icon.addPixmap(render(id, color, size, ratio), QIcon::Normal);
    return icon;
}

QIcon makeActive(Id id, const QColor& color, int size)
{
    QIcon icon;
    icon.addPixmap(render(id, color, size, 1.0), QIcon::Normal);
    return icon;
}

} // namespace icons
} // namespace elkbledom
