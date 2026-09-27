#pragma once

// Вспомогательные фабрики виджетов редактора: нужны и AmbiEditor.cpp, и
// AmbiEditorPanels.cpp, поэтому живут в заголовке.

#include "AmbiCanvas.h"
#include "EditorIcons.h"

#include <QColor>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QSettings>
#include <QSlider>
#include <QString>
#include <QToolButton>

namespace elkbledom {

namespace {

QLabel* sectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", "section");
    return label;
}
QLabel* valueLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", "value");
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return label;
}
QToolButton* actionButton(QWidget* parent, icons::Id id, const QString& text, const QString& tip)
{
    auto* button = new QToolButton(parent);
    button->setIcon(icons::make(id));
    button->setIconSize(QSize(18, 18));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setText(text);
    if (!tip.isEmpty()) button->setToolTip(tip);
    button->setAutoRaise(false);
    button->setFocusPolicy(Qt::NoFocus);
    button->setProperty("action", true);
    return button;
}
QToolButton* bareIconButton(QWidget* parent, icons::Id id, const QString& tip, bool checkable)
{
    auto* button = new QToolButton(parent);
    button->setIcon(icons::make(id));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(tip);
    button->setCheckable(checkable);
    button->setAutoRaise(true);
    button->setFixedSize(30, 28);
    button->setFocusPolicy(Qt::NoFocus);
    button->setProperty("bare", true);
    return button;
}
// Значок пресета: схематичная рамка того, что захватывается. Рисуется вручную,
// чтобы не тащить картинки для каждой области.
QIcon regionIcon(const QString& key, const QColor& color)
{
    QPixmap pixmap(26, 18);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.4));
    painter.setBrush(Qt::NoBrush);
    const QRectF box(1.5, 1.5, 23, 15);
    const double band = 0.34;
    if (key == QStringLiteral("full")) {
        painter.drawRect(box);
    } else if (key == QStringLiteral("top")) {
        painter.drawRect(box);
        painter.drawRect(QRectF(box.left(), box.top(), box.width(), box.height() * band));
    } else if (key == QStringLiteral("bottom")) {
        painter.drawRect(box);
        painter.drawRect(QRectF(box.left(), box.bottom() - box.height() * band, box.width(),
                               box.height() * band));
    } else if (key == QStringLiteral("left")) {
        painter.drawRect(box);
        painter.drawRect(QRectF(box.left(), box.top(), box.width() * band, box.height()));
    } else if (key == QStringLiteral("right")) {
        painter.drawRect(box);
        painter.drawRect(QRectF(box.right() - box.width() * band, box.top(), box.width() * band,
                                box.height()));
    } else if (key == QStringLiteral("center")) {
        painter.drawRect(box);
        const double t = (1.0 - band) / 2.0;
        painter.drawRect(QRectF(box.left() + box.width() * t, box.top() + box.height() * t,
                                box.width() * (1.0 - 2 * t), box.height() * (1.0 - 2 * t)));
    } else if (key == QStringLiteral("grid_3x3") || key == QStringLiteral("grid_5x3")) {
        painter.drawRect(box);
        const int columns = key == QStringLiteral("grid_3x3") ? 3 : 5;
        for (int c = 1; c < columns; ++c)
            painter.drawLine(QPointF(box.left() + box.width() * c / columns, box.top()),
                             QPointF(box.left() + box.width() * c / columns, box.bottom()));
        for (int row = 1; row < 3; ++row)
            painter.drawLine(QPointF(box.left(), box.top() + box.height() * row / 3.0),
                             QPointF(box.right(), box.top() + box.height() * row / 3.0));
    } else if (key == QStringLiteral("corners")) {
        painter.drawRect(box);
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 90));
        const double cw = box.width() * 0.34, ch = box.height() * 0.42;
        painter.drawRect(QRectF(box.left(), box.top(), cw, ch));
        painter.drawRect(QRectF(box.right() - cw, box.top(), cw, ch));
        painter.drawRect(QRectF(box.left(), box.bottom() - ch, cw, ch));
        painter.drawRect(QRectF(box.right() - cw, box.bottom() - ch, cw, ch));
    } else {
        painter.drawRect(box);
    }
    return QIcon(pixmap);
}
// Строка настройки в одну линию: подпись слева, ползунок в середине, значение справа.
QHBoxLayout* settingRow(QWidget* parent, const QString& caption, const QString& tip,
                        QSlider*& slider, QLabel*& value, int min, int max, int initial)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    auto* label = new QLabel(caption, parent);
    label->setToolTip(tip);
    label->setMinimumWidth(140);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(min, max);
    slider->setValue(initial);
    slider->setToolTip(tip);
    value = valueLabel(QString::number(initial), parent);
    value->setMinimumWidth(58);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(label);
    row->addWidget(slider, 1);
    row->addWidget(value);
    return row;
}
// Значок и подсказка инструмента расширенного режима.
icons::Id toolIcons(AmbiCanvas::Tool tool)
{
    switch (tool) {
    case AmbiCanvas::Tool::Move: return icons::Id::Select;
    case AmbiCanvas::Tool::Rectangle: return icons::Id::Rectangle;
    case AmbiCanvas::Tool::Zoom: return icons::Id::ZoomIn;
    case AmbiCanvas::Tool::Hand: return icons::Id::Pan;
    }
    return icons::Id::Select;
}
QString toolToolTip(AmbiCanvas::Tool tool)
{
    switch (tool) {
    case AmbiCanvas::Tool::Move:
        return QCoreApplication::translate("AmbiEditor", "Move and resize layers (V)");
    case AmbiCanvas::Tool::Rectangle:
        return QCoreApplication::translate("AmbiEditor",
                                           "Rectangular selection (M). Shift keeps a square, "
                                           "Alt draws from the centre");
    case AmbiCanvas::Tool::Zoom:
        return QCoreApplication::translate("AmbiEditor", "Zoom in, Alt+click to zoom out (Z)");
    case AmbiCanvas::Tool::Hand:
        return QCoreApplication::translate("AmbiEditor", "Pan the image (H, or hold Space)");
    }
    return QString();
}
QSettings viewStore()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("Magic Home Controller"), QStringLiteral("editorView"));
}

} // namespace

} // namespace elkbledom

