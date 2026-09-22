#pragma once

#include <QColor>
#include <QIcon>
#include <QString>
#include <QVector>

namespace elkbledom {

// Runtime icon generator. No resource files or application state are required.
class IconFactory final
{
public:
    IconFactory() = delete;

    static QIcon appIcon(int size = 256);
    static QIcon rainbowIcon(int size = 64, bool appStyle = false);
    static QIcon trayIcon(const QColor& color,
                          const QVector<QColor>& palette,
                          const QString& effect,
                          int size = 64);
    static QIcon trayIcon(int red, int green, int blue,
                          const QVector<QColor>& palette,
                          const QString& effect,
                          int size = 64);

};

} // namespace elkbledom
