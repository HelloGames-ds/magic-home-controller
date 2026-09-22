#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace elkbledom {

// Генератор одиннадцати программных эффектов с частотой 10 Гц по умолчанию.
class EffectEngine final : public QObject
{
    Q_OBJECT

public:
    explicit EffectEngine(QObject* parent = nullptr);

    static const QStringList& effects();
    static const QHash<QString, QString>& labels();
    static const QHash<QString, QString>& descriptions();
    // 0 — палитра не нужна, положительное число — нужное число цветов,
    // -1 — используется вся доступная палитра.
    static const QHash<QString, int>& paletteUsage();

    QString effect() const { return effect_; }
    QList<QColor> palette() const { return palette_; }
    double speed() const noexcept { return speed_; }
    double intensity() const noexcept { return intensity_; }
    double noise() const noexcept { return noise_; }
    bool reverse() const noexcept { return reverse_; }
    bool isActive() const noexcept { return timer_.isActive(); }

public Q_SLOTS:
    void setEffect(const QString& name);
    void setPalette(const QList<QColor>& palette);
    void setSpeed(double speed);
    void setIntensity(double intensity);
    void setNoise(double noise);
    void setReverse(bool reverse);
    void setInterval(int milliseconds);
    void start();
    void stop();

Q_SIGNALS:
    void colorChanged(int red, int green, int blue);

private Q_SLOTS:
    void tick();

private:
    static QColor interpolate(const QColor& first, const QColor& second,
                              double factor);
    QColor paletteAt(double position) const;
    void emitIfChanged(const QColor& color);

    QTimer timer_;
    QElapsedTimer elapsed_;
    QString effect_{QStringLiteral("static")};
    QList<QColor> palette_{QColor(191, 0, 255), QColor(0, 255, 255),
                           QColor(255, 255, 255), QColor(0, 0, 0)};
    double speed_ = 1.0;
    double intensity_ = 1.0;
    double noise_ = 0.0;
    bool reverse_ = false;
    QColor flashColor_;
    qint64 flashTimeMs_ = 0;
    QColor lastColor_;
    bool hasLastColor_ = false;
};

} // namespace elkbledom
