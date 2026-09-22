#include "IconFactory.h"

#include <QBrush>
#include <QConicalGradient>
#include <QPainter>
#include <QPen>
#include <QPixmap>

#include <algorithm>

namespace elkbledom {
namespace {

QConicalGradient hsvConical(int size)
{
    QConicalGradient gradient(size / 2.0, size / 2.0, 90.0);
    gradient.setColorAt(0.0, QColor(255, 0, 0));
    gradient.setColorAt(1.0 / 6.0, QColor(255, 0, 255));
    gradient.setColorAt(2.0 / 6.0, QColor(0, 0, 255));
    gradient.setColorAt(3.0 / 6.0, QColor(0, 255, 255));
    gradient.setColorAt(4.0 / 6.0, QColor(0, 255, 0));
    gradient.setColorAt(5.0 / 6.0, QColor(255, 255, 0));
    gradient.setColorAt(1.0, QColor(255, 0, 0));
    return gradient;
}

QColor validColor(const QColor& color)
{
    return color.isValid() ? color : QColor(0, 0, 0);
}

} // namespace

QIcon IconFactory::appIcon(int size)
{
    size = std::max(1, size);
    return rainbowIcon(size, true);
}

QIcon IconFactory::rainbowIcon(int size, bool appStyle)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    if (appStyle) {
        const int margin = static_cast<int>(size * 0.04);
        const int side = size - 2 * margin;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(30, 30, 36));
        painter.drawEllipse(margin, margin, side, side);

        const int inner = static_cast<int>(size * 0.12);
        painter.setBrush(hsvConical(size));
        painter.drawEllipse(inner, inner, size - 2 * inner, size - 2 * inner);

        const int center = static_cast<int>(size * 0.33);
        painter.setBrush(QColor(240, 240, 250));
        painter.drawEllipse(center, center, size - 2 * center, size - 2 * center);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(QStringLiteral("#b070ff")), std::max(2, size / 64)));
        painter.drawEllipse(margin, margin, side, side);
    } else {
        const int margin = 2;
        const int side = size - 2 * margin;
        const int ring = std::max(3, size / 12);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(30, 30, 36));
        painter.drawEllipse(margin, margin, side, side);
        painter.setBrush(hsvConical(size));
        painter.drawEllipse(margin + ring, margin + ring,
                            side - 2 * ring, side - 2 * ring);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(QStringLiteral("#b070ff")), 1));
        painter.drawEllipse(margin, margin, side, side);
    }
    return QIcon(pixmap);
}

QIcon IconFactory::trayIcon(const QColor& color,
                            const QVector<QColor>& palette,
                            const QString& effect,
                            int size)
{
    size = std::max(1, size);
    if (effect == QStringLiteral("rainbow"))
        return rainbowIcon(size, false);

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const int margin = 2;
    const int side = size - 2 * margin;
    const int ring = std::max(3, size / 12);
    const int innerMargin = margin + ring;
    const int innerSide = side - 2 * ring;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 30, 36));
    painter.drawEllipse(margin, margin, side, side);

    if (effect == QStringLiteral("static") || palette.size() <= 1) {
        painter.setBrush(validColor(color));
        painter.drawEllipse(innerMargin, innerMargin, innerSide, innerSide);
    } else {
        const qreal segment = 360.0 / palette.size();
        for (qsizetype index = 0; index < palette.size(); ++index) {
            painter.setBrush(validColor(palette.at(index)));
            painter.drawPie(innerMargin, innerMargin, innerSide, innerSide,
                            static_cast<int>(index * segment * 16.0),
                            static_cast<int>(segment * 16.0));
        }
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(QStringLiteral("#b070ff")), 1));
    painter.drawEllipse(margin, margin, side, side);
    return QIcon(pixmap);
}

QIcon IconFactory::trayIcon(int red, int green, int blue,
                            const QVector<QColor>& palette,
                            const QString& effect,
                            int size)
{
    return trayIcon(QColor(red, green, blue), palette, effect, size);
}

} // namespace elkbledom
