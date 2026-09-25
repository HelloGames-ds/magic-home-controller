#include "SettingsStore.h"

#include "protocol.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QtGlobal>

#include <cmath>

namespace elkbledom {
namespace {

constexpr auto AppName = "Magic Home Controller";

const QStringList ValidEffects {
    QStringLiteral("static"), QStringLiteral("breath"), QStringLiteral("rainbow"),
    QStringLiteral("gradient"), QStringLiteral("strobe"), QStringLiteral("pulse"),
    QStringLiteral("wave"), QStringLiteral("fire"), QStringLiteral("random_flash"),
    QStringLiteral("chase"), QStringLiteral("color_cycle")
};
const QStringList ValidRegions {
    QStringLiteral("top"), QStringLiteral("bottom"), QStringLiteral("center"),
    QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("full"),
    QStringLiteral("grid_3x3"), QStringLiteral("grid_5x3"),
    QStringLiteral("corner_tl"), QStringLiteral("corner_tr"),
    QStringLiteral("corner_bl"), QStringLiteral("corner_br"), QStringLiteral("custom")
};

bool finite(double value)
{
    return std::isfinite(value);
}

double bounded(double value, double fallback, double minimum, double maximum)
{
    return finite(value) ? qBound(minimum, value, maximum) : fallback;
}

QVector<QColor> decodePalette(const QString& text, const QVector<QColor>& fallback)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        return fallback;
    }

    QVector<QColor> result;
    const QJsonArray array = document.array();
    for (qsizetype i = 0; i < array.size() && result.size() < 4; ++i) {
        const QJsonArray rgb = array.at(i).toArray();
        if (rgb.size() != 3) {
            return fallback;
        }
        result.append(QColor(qBound(0, rgb.at(0).toInt(), 255),
                             qBound(0, rgb.at(1).toInt(), 255),
                             qBound(0, rgb.at(2).toInt(), 255)));
    }
    while (result.size() < 4) {
        result.append(QColor(0, 0, 0));
    }
    return result;
}

QString encodePalette(const QVector<QColor>& palette)
{
    QJsonArray outer;
    for (const QColor& color : palette) {
        outer.append(QJsonArray { color.red(), color.green(), color.blue() });
    }
    return QString::fromUtf8(QJsonDocument(outer).toJson(QJsonDocument::Compact));
}

QRectF decodeRect(const QString& text)
{
    const QStringList values = text.split(QLatin1Char(','));
    if (values.size() != 4) {
        return QRectF(0.0, 0.0, 1.0, 0.1);
    }
    bool ok[4] {};
    const double x = values[0].toDouble(&ok[0]);
    const double y = values[1].toDouble(&ok[1]);
    const double w = values[2].toDouble(&ok[2]);
    const double h = values[3].toDouble(&ok[3]);
    if (!ok[0] || !ok[1] || !ok[2] || !ok[3]
        || !finite(x) || !finite(y) || !finite(w) || !finite(h)) {
        return QRectF(0.0, 0.0, 1.0, 0.1);
    }
    const double safeX = qBound(0.0, x, 0.99);
    const double safeY = qBound(0.0, y, 0.99);
    return QRectF(safeX, safeY,
                  qBound(0.01, w, 1.0 - safeX),
                  qBound(0.01, h, 1.0 - safeY));
}

} // namespace

SettingsStore::SettingsStore(QObject* parent)
    : QObject(parent)
    , settings_(std::make_unique<QSettings>(configFilePath(), QSettings::IniFormat))
{
}

SettingsStore::~SettingsStore() = default;

QString SettingsStore::configFilePath()
{
    QString appData = qEnvironmentVariable("APPDATA");
    if (appData.isEmpty()) {
        appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    const QString directory = QDir(appData).filePath(QString::fromLatin1(AppName));
    QDir().mkpath(directory);
    return QDir(directory).filePath(QStringLiteral("config.ini"));
}

AppSettings SettingsStore::load() const
{
    AppSettings value;
    QString address = settings_->value(QStringLiteral("address")).toString();
    if (address.isEmpty()) {
        address = settings_->value(QStringLiteral("mac")).toString(); // legacy
    }
    address = address.trimmed();
    value.address = address.isEmpty() ? QString::fromLatin1(protocol::DefaultAddress) : address;

    value.color = QColor(settings_->value(QStringLiteral("color_r"), 191).toInt(),
                         settings_->value(QStringLiteral("color_g"), 0).toInt(),
                         settings_->value(QStringLiteral("color_b"), 255).toInt());
    const QVector<QColor> defaultPalette {
        value.color, QColor(0, 255, 255), QColor(255, 255, 255), QColor(0, 0, 0)
    };
    value.palette = decodePalette(settings_->value(QStringLiteral("palette")).toString(), defaultPalette);
    value.paletteCount = settings_->value(QStringLiteral("palette_count"), 2).toInt();
    value.brightness = settings_->value(QStringLiteral("brightness"), 100).toInt();
    value.mode = settings_->value(QStringLiteral("mode"), QStringLiteral("effect")).toString();
    value.effect = settings_->value(QStringLiteral("effect"), QStringLiteral("static")).toString();
    value.effectSpeed = settings_->value(QStringLiteral("effect_speed"), 1.0).toDouble();
    value.effectIntensity = settings_->value(QStringLiteral("effect_intensity"), 1.0).toDouble();
    value.effectNoise = settings_->value(QStringLiteral("effect_noise"), 0.0).toDouble();
    value.effectReverse = settings_->value(QStringLiteral("effect_reverse"), false).toBool();

    value.ambiRegion = settings_->value(QStringLiteral("ambi_region"), QStringLiteral("top")).toString();
    value.ambiBand = settings_->value(QStringLiteral("ambi_band"), 8).toInt();
    value.ambiBoost = settings_->value(QStringLiteral("ambi_boost"), 1.3).toDouble();
    value.ambiSmooth = settings_->value(QStringLiteral("ambi_smooth"), 0.3).toDouble();
    value.ambiMin = settings_->value(QStringLiteral("ambi_min"), 15).toInt();
    value.ambiAuto = settings_->value(QStringLiteral("ambi_auto"), false).toBool();
    value.ambiFreq = settings_->value(QStringLiteral("ambi_freq"), 3.0).toDouble();
    value.ambiScreen = settings_->value(QStringLiteral("ambi_screen"), 0).toInt();
    value.ambiCombine = settings_->value(QStringLiteral("ambi_combine"), QStringLiteral("average")).toString();
    value.ambiCapture = settings_->value(QStringLiteral("ambi_capture"), QStringLiteral("auto")).toString();
    value.ambiRect = decodeRect(settings_->value(QStringLiteral("ambi_rect"),
                                                  QStringLiteral("0.0,0.0,1.0,0.1")).toString());

    value.netInterval = settings_->value(QStringLiteral("net_interval"), 0.0).toDouble();
    value.netDedup = settings_->value(QStringLiteral("net_dedup"),
                                      settings_->value(QStringLiteral("ble_dedup"), 0.5)).toDouble();
    value.lastDevices = settings_->value(QStringLiteral("last_devices")).toStringList();
    value.restorePower = settings_->value(QStringLiteral("restore_power"), true).toBool();
    value.lastPowerOn = settings_->value(QStringLiteral("power_on"), false).toBool();
    value.powerOffOnExit = settings_->value(QStringLiteral("power_off_on_exit"), false).toBool();
    value.powerOffOnShutdown = settings_->value(QStringLiteral("power_off_on_shutdown"), false).toBool();
    value.smoothEnabled = settings_->value(QStringLiteral("smooth_enabled"), true).toBool();
    value.smoothTau = settings_->value(QStringLiteral("smooth_tau"), 200).toInt();
    value.loggingEnabled = settings_->value(QStringLiteral("logging_enabled"), true).toBool();
    value.autostart = settings_->value(QStringLiteral("autostart"), false).toBool();
    value.startMinimized = settings_->value(QStringLiteral("start_minimized"), false).toBool();
    value.saveOnExit = settings_->value(QStringLiteral("save_on_exit"), true).toBool();
    value.keepalive = settings_->value(QStringLiteral("keepalive"), true).toBool();
    value.language = settings_->value(QStringLiteral("language"), QStringLiteral("en")).toString();
    return validated(value);
}

bool SettingsStore::save(const AppSettings& input)
{
    const AppSettings value = validated(input);
    settings_->setValue(QStringLiteral("address"), value.address);
    settings_->setValue(QStringLiteral("color_r"), value.color.red());
    settings_->setValue(QStringLiteral("color_g"), value.color.green());
    settings_->setValue(QStringLiteral("color_b"), value.color.blue());
    settings_->setValue(QStringLiteral("palette"), encodePalette(value.palette));
    settings_->setValue(QStringLiteral("palette_count"), value.paletteCount);
    settings_->setValue(QStringLiteral("brightness"), value.brightness);
    settings_->setValue(QStringLiteral("mode"), value.mode);
    settings_->setValue(QStringLiteral("effect"), value.effect);
    settings_->setValue(QStringLiteral("effect_speed"), value.effectSpeed);
    settings_->setValue(QStringLiteral("effect_intensity"), value.effectIntensity);
    settings_->setValue(QStringLiteral("effect_noise"), value.effectNoise);
    settings_->setValue(QStringLiteral("effect_reverse"), value.effectReverse);
    settings_->setValue(QStringLiteral("ambi_region"), value.ambiRegion);
    settings_->setValue(QStringLiteral("ambi_band"), value.ambiBand);
    settings_->setValue(QStringLiteral("ambi_boost"), value.ambiBoost);
    settings_->setValue(QStringLiteral("ambi_smooth"), value.ambiSmooth);
    settings_->setValue(QStringLiteral("ambi_min"), value.ambiMin);
    settings_->setValue(QStringLiteral("ambi_auto"), value.ambiAuto);
    settings_->setValue(QStringLiteral("ambi_freq"), value.ambiFreq);
    settings_->setValue(QStringLiteral("ambi_screen"), value.ambiScreen);
    settings_->setValue(QStringLiteral("ambi_combine"), value.ambiCombine);
    settings_->setValue(QStringLiteral("ambi_capture"), value.ambiCapture);
    settings_->setValue(QStringLiteral("ambi_rect"),
                        QStringLiteral("%1,%2,%3,%4")
                            .arg(value.ambiRect.x()).arg(value.ambiRect.y())
                            .arg(value.ambiRect.width()).arg(value.ambiRect.height()));
    settings_->setValue(QStringLiteral("net_interval"), value.netInterval);
    settings_->setValue(QStringLiteral("net_dedup"), value.netDedup);
    settings_->setValue(QStringLiteral("last_devices"), value.lastDevices);
    settings_->setValue(QStringLiteral("restore_power"), value.restorePower);
    settings_->setValue(QStringLiteral("power_on"), value.lastPowerOn);
    settings_->setValue(QStringLiteral("power_off_on_exit"), value.powerOffOnExit);
    settings_->setValue(QStringLiteral("power_off_on_shutdown"), value.powerOffOnShutdown);
    settings_->setValue(QStringLiteral("smooth_enabled"), value.smoothEnabled);
    settings_->setValue(QStringLiteral("smooth_tau"), value.smoothTau);
    settings_->setValue(QStringLiteral("logging_enabled"), value.loggingEnabled);
    settings_->setValue(QStringLiteral("autostart"), value.autostart);
    settings_->setValue(QStringLiteral("start_minimized"), value.startMinimized);
    settings_->setValue(QStringLiteral("save_on_exit"), value.saveOnExit);
    settings_->setValue(QStringLiteral("keepalive"), value.keepalive);
    settings_->setValue(QStringLiteral("language"), value.language);
    settings_->sync();

    if (settings_->status() != QSettings::NoError) {
        Q_EMIT error(tr("Failed to save settings: %1").arg(configFilePath()));
        return false;
    }
    return applyAutostart(value.autostart);
}

bool SettingsStore::applyAutostart(bool enabled, QString* errorMessage)
{
#ifdef Q_OS_WIN
    const QString name = QString::fromLatin1(AppName);
    const QString executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    QSettings userRun(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                      QSettings::NativeFormat);
    if (enabled) {
        userRun.setValue(name, QStringLiteral("\"%1\" --minimized").arg(executable));
    } else {
        userRun.remove(name);
    }
    userRun.sync();
    if (userRun.status() != QSettings::NoError) {
        const QString message = tr("Failed to change startup entry in HKCU");
        if (errorMessage) {
            *errorMessage = message;
        }
        Q_EMIT error(message);
        return false;
    }

    // Старые версии и установщик писали автозапуск в HKLM под собственным
    // именем; из-за этого галочка в HKCU ничего не отключала. Чистим и его.
    QSettings machineRun(QStringLiteral("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                         QSettings::NativeFormat);
    bool machineChanged = false;
    if (machineRun.contains(name) || machineRun.contains(QStringLiteral("ELK-BLEDOM"))) {
        machineRun.remove(name);
        machineRun.remove(QStringLiteral("ELK-BLEDOM"));
        machineChanged = true;
    }
    if (machineChanged) {
        machineRun.sync();
        if (machineRun.status() != QSettings::NoError) {
            const QString message = tr("Startup for all users (HKLM) could not be removed - administrator rights are required.");
            if (errorMessage) {
                *errorMessage = message;
            }
            Q_EMIT error(message);
        }
    }
#else
    Q_UNUSED(enabled)
    Q_UNUSED(errorMessage)
#endif
    return true;
}

void SettingsStore::setLastPowerOn(bool on)
{
    settings_->setValue(QStringLiteral("power_on"), on);
    settings_->sync();
}

AppSettings SettingsStore::validated(const AppSettings& input)
{
    AppSettings value = input;
    value.address = value.address.trimmed();
    if (value.address.isEmpty()) {
        value.address = QString::fromLatin1(protocol::DefaultAddress);
    }
    QHostAddress hostAddress(value.address);
    if (hostAddress.isNull() || hostAddress.protocol() != QAbstractSocket::IPv4Protocol) {
        value.address = QString::fromLatin1(protocol::DefaultAddress);
    }
    value.color.setRed(qBound(0, value.color.red(), 255));
    value.color.setGreen(qBound(0, value.color.green(), 255));
    value.color.setBlue(qBound(0, value.color.blue(), 255));
    if (value.palette.isEmpty()) {
        value.palette = { value.color, QColor(0, 255, 255), QColor(255, 255, 255), QColor(0, 0, 0) };
    }
    value.palette.resize(4);
    for (QColor& color : value.palette) {
        if (!color.isValid()) {
            color = QColor(0, 0, 0);
        }
    }
    value.paletteCount = qBound(2, value.paletteCount, 4);
    value.brightness = qBound(1, value.brightness, 100);
    if (value.mode != QStringLiteral("effect") && value.mode != QStringLiteral("ambilight")) {
        value.mode = QStringLiteral("effect");
    }
    if (!ValidEffects.contains(value.effect)) {
        value.effect = QStringLiteral("static");
    }
    value.effectSpeed = bounded(value.effectSpeed, 1.0, 0.05, 5.0);
    value.effectIntensity = bounded(value.effectIntensity, 1.0, 0.1, 1.0);
    value.effectNoise = bounded(value.effectNoise, 0.0, 0.0, 1.0);

    if (!ValidRegions.contains(value.ambiRegion)) {
        value.ambiRegion = QStringLiteral("top");
    }
    value.ambiBand = qBound(1, value.ambiBand, 80);
    value.ambiBoost = bounded(value.ambiBoost, 1.3, 1.0, 3.0);
    value.ambiSmooth = bounded(value.ambiSmooth, 0.3, 0.0, 0.9);
    value.ambiMin = qBound(0, value.ambiMin, 120);
    value.ambiFreq = bounded(value.ambiFreq, 3.0, 1.0, 20.0);
    value.ambiScreen = qMax(0, value.ambiScreen);
    if (value.ambiCombine != QStringLiteral("average")
        && value.ambiCombine != QStringLiteral("brightest")
        && value.ambiCombine != QStringLiteral("saturated")) {
        value.ambiCombine = QStringLiteral("average");
    }
    value.ambiRect = decodeRect(QStringLiteral("%1,%2,%3,%4")
                                    .arg(value.ambiRect.x()).arg(value.ambiRect.y())
                                    .arg(value.ambiRect.width()).arg(value.ambiRect.height()));
    if (value.ambiCapture != QStringLiteral("auto")
        && value.ambiCapture != QStringLiteral("dxgi")
        && value.ambiCapture != QStringLiteral("gdi")
        && value.ambiCapture != QStringLiteral("wgc")) {
        value.ambiCapture = QStringLiteral("auto");
    }
    value.netInterval = bounded(value.netInterval, 0.0, 0.0, 2.0);
    value.netDedup = bounded(value.netDedup, 0.5, 0.0, 5.0);
    value.smoothTau = qBound(20, value.smoothTau, 500);
    QStringList recent;
    for (const QString& device : value.lastDevices) {
        const QString trimmed = device.trimmed();
        if (trimmed.isEmpty() || recent.contains(trimmed)) continue;
        recent.append(trimmed);
        if (recent.size() >= 5) break;
    }
    value.lastDevices = recent;
    if (value.language != QStringLiteral("en") && value.language != QStringLiteral("ru")) {
        value.language = QStringLiteral("en");
    }
    return value;
}

} // namespace elkbledom
