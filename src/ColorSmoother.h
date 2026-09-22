#pragma once

#include <QColor>
#include <QObject>
#include <QTimer>

namespace elkbledom {

// EMA-сглаживание в HSV по кратчайшему пути оттенка.
class ColorSmoother final : public QObject
{
    Q_OBJECT

public:
    explicit ColorSmoother(QObject* parent = nullptr, int intervalMs = 40,
                           int tauMs = 200);

    bool isEnabled() const noexcept { return enabled_; }
    QColor current() const noexcept { return current_; }
    QColor target() const noexcept { return target_; }

public Q_SLOTS:
    void setEnabled(bool enabled);
    void setTau(int tauMs);
    void setCurrent(const QColor& rgb);
    void setCurrent(int red, int green, int blue);
    void animateTo(const QColor& rgb);
    void animateTo(int red, int green, int blue);

Q_SIGNALS:
    void colorChanged(int red, int green, int blue);

private Q_SLOTS:
    void tick();

private:
    QTimer timer_;
    int intervalMs_ = 40;
    double tauSeconds_ = 0.2;
    QColor current_{0, 0, 0};
    QColor target_{0, 0, 0};
    QColor lastEmitted_;
    bool enabled_ = true;
    bool hasLastEmitted_ = false;
};

} // namespace elkbledom
