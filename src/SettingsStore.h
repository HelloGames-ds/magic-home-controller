#pragma once

#include <QColor>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

class QSettings;

namespace elkbledom {

struct AppSettings
{
    QString address = QStringLiteral("192.168.1.100");
    QColor color = QColor(191, 0, 255);
    QVector<QColor> palette {
        QColor(191, 0, 255), QColor(0, 255, 255),
        QColor(255, 255, 255), QColor(0, 0, 0)
    };
    int paletteCount = 2;
    int brightness = 100;
    QString mode = QStringLiteral("effect");
    QString effect = QStringLiteral("static");
    double effectSpeed = 1.0;
    double effectIntensity = 1.0;
    double effectNoise = 0.0;
    bool effectReverse = false;

    QString ambiRegion = QStringLiteral("top");
    int ambiBand = 8;
    double ambiBoost = 1.3;
    double ambiSmooth = 0.3;
    int ambiMin = 15;
    bool ambiAuto = false;
    double ambiFreq = 3.0;
    int ambiScreen = 0;
    QString ambiCombine = QStringLiteral("average");
    QRectF ambiRect = QRectF(0.0, 0.0, 1.0, 0.1);
    QString ambiCapture = QStringLiteral("auto");

    QStringList lastDevices;
    bool restorePower = true;
    bool lastPowerOn = false;
    bool powerOffOnExit = false;
    bool powerOffOnShutdown = false;

    double netInterval = 0.0;
    double netDedup = 0.5;
    bool smoothEnabled = true;
    int smoothTau = 200;
    bool loggingEnabled = true;
    bool autostart = false;
    bool startMinimized = false;
    bool saveOnExit = true;
    bool keepalive = true;
    QString language = QStringLiteral("en");
};

class SettingsStore final : public QObject
{
    Q_OBJECT

public:
    explicit SettingsStore(QObject* parent = nullptr);
    ~SettingsStore() override;

    static QString configFilePath();

    AppSettings load() const;
    bool save(const AppSettings& settings);
    bool applyAutostart(bool enabled, QString* errorMessage = nullptr);
    void setLastPowerOn(bool on);

Q_SIGNALS:
    void error(const QString& message);

private:
    static AppSettings validated(const AppSettings& settings);

    std::unique_ptr<QSettings> settings_;
};

} // namespace elkbledom
