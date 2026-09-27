#include "AmbiLight.h"

#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QScreen>
#include <QTimer>
#include <QtMath>
#include <algorithm>

#include "AppLog.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "DxgiCapture.h"
#include "WgcCapture.h"
#endif

namespace elkbledom {
namespace {
constexpr int kClipDelta = 60;

#ifdef Q_OS_WIN
// Физический прямоугольник монитора в координатах виртуального рабочего стола.
// QScreen::name() на некоторых системах отдаёт модель монитора, а не устройство
// («\\\\.\\DISPLAY1»), поэтому точный матчинг по имени не работает. Считаем
// приблизительный rect из Qt-геометрии (позиция*dpr), затем «прищёлкиваем» к
// реальному MONITORINFO.rcMonitor с максимальным пересечением — это работает и
// при мониторах с отрицательными координатами, и при одинаковом размере.
QRect physicalScreenRect(QScreen* screen)
{
    const qreal dpr = screen->devicePixelRatio();
    const QRect logical = screen->geometry();
    const QRect approx(qRound(logical.x() * dpr), qRound(logical.y() * dpr),
                       qRound(logical.width() * dpr), qRound(logical.height() * dpr));
    if (approx.isEmpty()) {
        return approx;
    }

    struct Best
    {
        QRect approx;
        QRect rc;
        qint64 overlap = -1;
    } best{ approx };

    EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR monitor, HDC, LPRECT, LPARAM lp) -> BOOL {
            auto* b = reinterpret_cast<Best*>(lp);
            MONITORINFOEXW info{};
            info.cbSize = sizeof(info);
            if (GetMonitorInfoW(monitor, &info)) {
                const RECT& r = info.rcMonitor;
                const QRect rc(r.left, r.top, r.right - r.left, r.bottom - r.top);
                const QRect inter = rc.intersected(b->approx);
                const qint64 overlap = qint64(inter.width()) * qint64(inter.height());
                if (overlap > b->overlap) {
                    b->rc = rc;
                    b->overlap = overlap;
                }
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&best));

    if (best.overlap > 0) {
        return best.rc;
    }
    return approx;
}
#endif

int clippedChannel(int value, int previous)
{
    return std::clamp(value, previous - kClipDelta, previous + kClipDelta);
}

// Контраст относительно среднего канала: ровно то, что делал boost амбилайта.
QColor boostColor(const QColor& raw, double boost)
{
    if (!raw.isValid()) return {};
    const double average = (raw.red() + raw.green() + raw.blue()) / 3.0;
    int red = std::clamp(static_cast<int>(average + (raw.red() - average) * boost), 0, 255);
    int green = std::clamp(static_cast<int>(average + (raw.green() - average) * boost), 0, 255);
    int blue = std::clamp(static_cast<int>(average + (raw.blue() - average) * boost), 0, 255);
    return {red, green, blue};
}

// Множители яркости и насыщенности поверх boost (0 — чёрный, 1 — без изменений).
QColor adjustZoneColor(const QColor& color, double brightness, double saturation)
{
    if (!color.isValid()) return {};
    const auto channel = [brightness, saturation](int value, int other1, int other2) {
        const double average = (value + other1 + other2) / 3.0;
        const double boosted = average + (value - average) * saturation;
        return std::clamp(static_cast<int>(boosted * brightness), 0, 255);
    };
    return {channel(color.red(), color.green(), color.blue()),
            channel(color.green(), color.red(), color.blue()),
            channel(color.blue(), color.red(), color.green())};
}

// Отсечка тёмного и нормализация яркости поверх boost.
QColor applyLevels(const QColor& color, int minLevel, bool autoBright)
{
    if (!color.isValid()) return {};
    const double average = (color.red() + color.green() + color.blue()) / 3.0;
    if (average < minLevel) return {0, 0, 0};
    int red = color.red();
    int green = color.green();
    int blue = color.blue();
    const int maximum = std::max({red, green, blue});
    if (autoBright && maximum > 20 && maximum < 255) {
        const double scale = 255.0 / maximum;
        red = std::min(255, static_cast<int>(red * scale));
        green = std::min(255, static_cast<int>(green * scale));
        blue = std::min(255, static_cast<int>(blue * scale));
    }
    return {red, green, blue};
}
} // namespace

AmbiLight::AmbiLight(QObject* parent) : QObject(parent), timer_(new QTimer(this))
{
    timer_->setInterval(300);
    connect(timer_, &QTimer::timeout, this, &AmbiLight::capture);
}

AmbiLight::~AmbiLight() = default;

const QHash<QString, QString>& AmbiLight::regions()
{
    static const QHash<QString, QString> value{
        {"top", tr("Top band")}, {"bottom", tr("Bottom band")},
        {"center", tr("Center band")}, {"left", tr("Left band")},
        {"right", tr("Right band")}, {"full", tr("Full screen")},
        {"grid_3x3", tr("Grid 3×3")}, {"grid_5x3", tr("Grid 5×3")},
    {"corner_tl", tr("Top-left corner")}, {"corner_tr", tr("Top-right corner")},
    {"corner_bl", tr("Bottom-left corner")}, {"corner_br", tr("Bottom-right corner")},
    {"corners", tr("All four corners")},
    {"custom", tr("Custom rectangle")}};
    return value;
}

const QHash<QString, QString>& AmbiLight::combineModes()
{
    static const QHash<QString, QString> value{{"average", tr("Average of samples")},
                                               {"brightest", tr("Brightest")},
                                               {"saturated", tr("Most saturated")}};
    return value;
}

QVector<AmbiLight::ScreenInfo> AmbiLight::listScreens()
{
    QVector<ScreenInfo> result;
    const auto screens = QGuiApplication::screens();
    QScreen* primary = QGuiApplication::primaryScreen();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen* screen = screens.at(i);
        result.push_back({i, screen->name().isEmpty() ? tr("Screen %1").arg(i + 1)
                                                       : screen->name(),
                          screen->geometry().size(), screen == primary});
    }
    return result;
}

void AmbiLight::setRegion(const QString& value)
{
    if (regions().contains(value)) region_ = value;
}
void AmbiLight::setBandPct(int value) { bandPct_ = std::clamp(value, 1, 80); }
void AmbiLight::setBoost(double value) { boost_ = std::clamp(value, 1.0, 3.0); }
void AmbiLight::setSmooth(double value) { smooth_ = std::clamp(value, 0.0, 0.9); }
void AmbiLight::setMinLevel(int value) { minLevel_ = std::clamp(value, 0, 120); }
void AmbiLight::setAutoBright(bool value) { autoBright_ = value; }
void AmbiLight::setFrequency(double hz)
{
    frequency_ = std::clamp(hz, 1.0, 30.0);
    timer_->setInterval(std::max(30, qRound(1000.0 / frequency_)));
}
void AmbiLight::setScreenIndex(int index)
{
    screenIndex_ = std::max(0, index);
    selectScreen(); // Переключаем источник сразу, а не только при следующем start().
}
void AmbiLight::setCustomRect(double x, double y, double width, double height)
{
    x = std::clamp(x, 0.0, 1.0);
    y = std::clamp(y, 0.0, 1.0);
    width = std::clamp(width, 0.01, 1.0 - x);
    height = std::clamp(height, 0.01, 1.0 - y);
    customRect_ = {x, y, width, height};
}
void AmbiLight::setCustomRect(const QRectF& rect)
{
    setCustomRect(rect.x(), rect.y(), rect.width(), rect.height());
}
void AmbiLight::setCombine(const QString& value)
{
    if (combineModes().contains(value)) combine_ = value;
}

void AmbiLight::setZones(const QVector<CaptureZone>& zones)
{
    zones_ = zones;
    zoneRasters_.clear();
    zoneRasterSize_ = {};
    zoneLast_.clear();
    zoneHasLast_.clear();
}

bool AmbiLight::hasZones() const
{
    for (const CaptureZone& zone : zones_) {
        if (zone.enabled && !zone.isEmpty()) return true;
    }
    return false;
}

QString AmbiLight::captureModeName(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Auto: return tr("Auto (recommended)");
    case CaptureMode::Dxgi: return tr("DXGI (hardware)");
    case CaptureMode::Gdi: return tr("GDI (legacy)");
    case CaptureMode::Wgc: return tr("Windows.Graphics.Capture (hardware)");
    }
    return {};
}

QString AmbiLight::captureModeKey(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Auto: return QStringLiteral("auto");
    case CaptureMode::Dxgi: return QStringLiteral("dxgi");
    case CaptureMode::Gdi: return QStringLiteral("gdi");
    case CaptureMode::Wgc: return QStringLiteral("wgc");
    }
    return QStringLiteral("auto");
}

CaptureMode AmbiLight::captureModeFromString(const QString& key)
{
    if (key == QLatin1String("dxgi")) return CaptureMode::Dxgi;
    if (key == QLatin1String("gdi")) return CaptureMode::Gdi;
    if (key == QLatin1String("wgc")) return CaptureMode::Wgc;
    return CaptureMode::Auto;
}

void AmbiLight::setCaptureMode(CaptureMode mode)
{
    if (captureMode_ == mode) return;
    captureMode_ = mode;
    selectScreen(); // Переинициализируем источник под новый режим.
}
bool AmbiLight::isRunning() const { return timer_->isActive(); }

void AmbiLight::selectScreen()
{
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        screen_ = nullptr;
        return;
    }
    screenIndex_ = std::clamp(screenIndex_, 0, static_cast<int>(screens.size()) - 1);
    screen_ = screens.at(screenIndex_);

#ifdef Q_OS_WIN
    backend_.reset();
    // Захват создаём только при включённом амбилайте: иначе даже выключенный
    // источник будет держать DXGI-дупликатор / WGC-сессию.
    if (screen_ && isRunning() && captureMode_ != CaptureMode::Gdi) {
        const QRect phys = physicalScreenRect(screen_);
        AppLog::logline(QStringLiteral("Захват: экран %1 (%2), физический прямоугольник %3x%4 @ %5,%6")
                            .arg(screenIndex_)
                            .arg(screen_->name())
                            .arg(phys.width())
                            .arg(phys.height())
                            .arg(phys.x())
                            .arg(phys.y()));
        if (captureMode_ == CaptureMode::Wgc) {
            auto backend = std::make_unique<WgcCapture>();
            if (backend->start(screenIndex_, phys)) {
                backend_ = std::move(backend);
            }
        } else {
            auto backend = std::make_unique<DxgiCapture>();
            if (backend->start(screenIndex_, phys)) {
                backend_ = std::move(backend);
            }
        }
        // Если инициализация не удалась — backend_ пуст, остаёмся на GDI.
    }
#endif
}

void AmbiLight::start()
{
    timer_->start();
    selectScreen(); // Бэкенд создаётся уже при работающем таймере.
#ifdef Q_OS_WIN
    AppLog::logline(backend_ && backend_->active()
                        ? QStringLiteral("Амбилайт включен (%1, %2x%3)")
                              .arg(backend_->name())
                              .arg(backend_->size().width())
                              .arg(backend_->size().height())
                        : QStringLiteral("Амбилайт включен (захват GDI)"));
#else
    AppLog::logline(QStringLiteral("Амбилайт включен (захват GDI)"));
#endif
    hasLast_ = false;
    last_ = QColor();
    rawLast_ = QColor();
    zoneLast_.clear();
    zoneHasLast_.clear();
    zoneRasters_.clear();
    zoneRasterSize_ = {};
#ifdef Q_OS_WIN
    hwStill_.invalidate();
#endif
    timer_->start();
}
void AmbiLight::stop()
{
    timer_->stop();
#ifdef Q_OS_WIN
    if (backend_) {
        backend_->shutdown();
    }
#endif
    AppLog::logline(QStringLiteral("Амбилайт выключен"));
}

QVector<QColor> AmbiLight::sampleGdi(const QVector<QRect>& rects) const
{
    QVector<QColor> samples;
    samples.reserve(rects.size());
    // Снимаем каждую область отдельно: вместо BitBlt всего экрана несколько
    // маленьких захватов заметно снижают нагрузку и уменьшают подёргивание
    // аппаратного курсора.
    for (const QRect& rect : rects) {
        const QPixmap shot = screen_->grabWindow(
            0, rect.x(), rect.y(), rect.width(), rect.height());
        if (shot.isNull()) continue;
        const QImage image = shot.toImage();
        if (image.width() < 1 || image.height() < 1) continue;
        qint64 red = 0, green = 0, blue = 0;
        int count = 0;
        for (int iy = 1; iy <= 3; ++iy) {
            for (int ix = 1; ix <= 3; ++ix) {
                const int x = image.width() * ix / 4;
                const int y = image.height() * iy / 4;
                const QColor pixel = image.pixelColor(
                    std::clamp(x, 0, image.width() - 1),
                    std::clamp(y, 0, image.height() - 1));
                red += pixel.red(); green += pixel.green(); blue += pixel.blue();
                ++count;
            }
        }
        if (count > 0)
            samples.push_back(QColor(int(red / count), int(green / count), int(blue / count)));
    }
    return samples;
}

#ifdef Q_OS_WIN
QVector<QColor> AmbiLight::sampleBackend(const QVector<QRect>& rects, const QSize& logicalSize) const
{
    QVector<QColor> samples;
    const QSize dxSize = backend_->size();
    if (dxSize.isEmpty()) {
        backend_->releaseFrame();
        return samples;
    }
    samples.reserve(rects.size());
    const double scaleX = double(dxSize.width()) / double(logicalSize.width());
    const double scaleY = double(dxSize.height()) / double(logicalSize.height());
    for (const QRect& rect : rects) {
        const QRect phys(qRound(rect.x() * scaleX), qRound(rect.y() * scaleY),
                         qRound(rect.width() * scaleX), qRound(rect.height() * scaleY));
        const QColor color = backend_->sampleArea(phys);
        if (color.isValid()) samples.push_back(color);
    }
    backend_->releaseFrame();
    return samples;
}
#endif

QVector<QRect> AmbiLight::buildRects(int w, int h, const QString& selectedRegion,
                                     int selectedBandPct, const QRectF& selectedCustom) const
{
    if (w <= 0 || h <= 0) return {};
    const QString r = selectedRegion.isEmpty() ? region_ : selectedRegion;
    const int bp = selectedBandPct < 0 ? bandPct_ : std::clamp(selectedBandPct, 1, 80);
    const QRectF cr = selectedCustom.isNull() ? customRect_ : selectedCustom;
    const int bandH = std::max(4, static_cast<int>(h * bp / 100.0));
    const int bandW = std::max(4, static_cast<int>(w * bp / 100.0));

    if (r == "top") return {{0, 0, w, bandH}};
    if (r == "bottom") return {{0, h - bandH, w, bandH}};
    if (r == "center") return {{0, (h - bandH) / 2, w, bandH}};
    if (r == "left") return {{0, 0, bandW, h}};
    if (r == "right") return {{w - bandW, 0, bandW, h}};
    if (r == "full") return {{0, 0, w, h}};

    const int cw = std::max(4, static_cast<int>(w * bp / 50.0));
    const int ch = std::max(4, static_cast<int>(h * bp / 50.0));
    if (r == "corner_tl") return {{0, 0, cw, ch}};
    if (r == "corner_tr") return {{w - cw, 0, cw, ch}};
    if (r == "corner_bl") return {{0, h - ch, cw, ch}};
    if (r == "corner_br") return {{w - cw, h - ch, cw, ch}};
    // Все четыре угла сразу — один пресет вместо четырёх кнопок.
    if (r == "corners") return {{0, 0, cw, ch}, {w - cw, 0, cw, ch},
                               {0, h - ch, cw, ch}, {w - cw, h - ch, cw, ch}};

    if (r == "grid_3x3" || r == "grid_5x3") {
        const int columns = r == "grid_3x3" ? 3 : 5;
        constexpr int rows = 3;
        const int cellW = w / columns;
        const int cellH = h / rows;
        const int insetX = std::max(2, cellW / 10);
        const int insetY = std::max(2, cellH / 10);
        QVector<QRect> result;
        result.reserve(columns * rows);
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < columns; ++x)
                result.push_back({x * cellW + insetX, y * cellH + insetY,
                                  cellW - 2 * insetX, cellH - 2 * insetY});
        return result;
    }
    if (r == "custom") {
        return {{static_cast<int>(cr.x() * w), static_cast<int>(cr.y() * h),
                 std::max(2, static_cast<int>(cr.width() * w)),
                 std::max(2, static_cast<int>(cr.height() * h))}};
    }
    return {{0, 0, w, bandH}};
}

QColor AmbiLight::combine(const QVector<QColor>& samples, const QString& selectedMode) const
{
    if (samples.isEmpty()) return {0, 0, 0};
    if (samples.size() == 1) return samples.constFirst();
    const QString mode = selectedMode.isEmpty() ? combine_ : selectedMode;
    if (mode == "brightest") {
        return *std::max_element(samples.cbegin(), samples.cend(), [](const QColor& a, const QColor& b) {
            return a.red() + a.green() + a.blue() < b.red() + b.green() + b.blue();
        });
    }
    if (mode == "saturated") {
        return *std::max_element(samples.cbegin(), samples.cend(), [](const QColor& a, const QColor& b) {
            const auto spread = [](const QColor& c) {
                return std::max({c.red(), c.green(), c.blue()}) - std::min({c.red(), c.green(), c.blue()});
            };
            return spread(a) < spread(b);
        });
    }
    qint64 red = 0, green = 0, blue = 0;
    for (const QColor& color : samples) {
        red += color.red(); green += color.green(); blue += color.blue();
    }
    return {static_cast<int>(red / samples.size()), static_cast<int>(green / samples.size()),
            static_cast<int>(blue / samples.size())};
}

QColor AmbiLight::combineSamples(const QVector<QColor>& samples, const QString& mode) const
{
    return combine(samples, mode);
}

QColor AmbiLight::applyBoost(const QColor& color, double boost)
{
    return boostColor(color, boost);
}

QColor AmbiLight::applyBrightnessSaturation(const QColor& color, double brightness,
                                            double saturation)
{
    return adjustZoneColor(color, brightness, saturation);
}

QColor AmbiLight::processRaw(const QColor& raw, double selectedBoost,
                             int selectedMinLevel, int selectedAutoBright) const
{
    const double b = selectedBoost < 0.0 ? boost_ : selectedBoost;
    const int minimum = selectedMinLevel < 0 ? minLevel_ : selectedMinLevel;
    const bool normalize = selectedAutoBright < 0 ? autoBright_ : selectedAutoBright != 0;
    return applyLevels(boostColor(raw, b), minimum, normalize);
}

void AmbiLight::rebuildZoneRasters(const QSize& size)
{
    if (zoneRasterSize_ == size && int(zoneRasters_.size()) == zones_.size()) return;
    zoneRasters_.clear();
    zoneRasters_.reserve(zones_.size());
    for (const CaptureZone& zone : zones_) {
        ZoneRaster raster;
        if (zone.enabled && !zone.isEmpty() && zone.rasterize(size, &raster.area, &raster.mask))
            zoneRasters_.push_back(raster);
        else
            zoneRasters_.push_back(ZoneRaster{});
    }
    zoneRasterSize_ = size;
    zoneLast_.clear();
    zoneHasLast_.clear();
}

QVector<QColor> AmbiLight::sampleZonesGdi(const QVector<ZoneRaster>& rasters) const
{
    QVector<QColor> colors;
    colors.reserve(rasters.size());
    for (const ZoneRaster& raster : rasters) {
        if (raster.area.isEmpty() || raster.mask.isNull()) {
            colors.push_back(QColor());
            continue;
        }
        const QPixmap shot = screen_->grabWindow(0, raster.area.x(), raster.area.y(),
                                                 raster.area.width(), raster.area.height());
        if (shot.isNull()) {
            colors.push_back(QColor());
            continue;
        }
        const QImage image = shot.toImage().convertToFormat(QImage::Format_RGB32);
        if (image.width() < 1 || image.height() < 1) {
            colors.push_back(QColor());
            continue;
        }
        const int stepX = std::max(1, image.width() / 8);
        const int stepY = std::max(1, image.height() / 8);
        const int maskWidth = raster.mask.width();
        const int maskHeight = raster.mask.height();
        quint64 red = 0, green = 0, blue = 0;
        int count = 0;
        for (int y = 0; y < image.height(); y += stepY) {
            const int maskY = std::clamp(y * maskHeight / image.height(), 0, maskHeight - 1);
            const auto* maskRow = reinterpret_cast<const uchar*>(raster.mask.constScanLine(maskY));
            for (int x = 0; x < image.width(); x += stepX) {
                const int maskX = std::clamp(x * maskWidth / image.width(), 0, maskWidth - 1);
                if (maskRow[maskX] == 0) continue;
                const QRgb pixel = image.pixel(x, y);
                red += qRed(pixel);
                green += qGreen(pixel);
                blue += qBlue(pixel);
                ++count;
            }
        }
        colors.push_back(count > 0 ? QColor(int(red / count), int(green / count), int(blue / count))
                                   : QColor());
    }
    return colors;
}

#ifdef Q_OS_WIN
QVector<QColor> AmbiLight::sampleZonesBackend(const QVector<ZoneRaster>& rasters,
                                              const QSize& logicalSize) const
{
    QVector<QColor> colors;
    const QSize dxSize = backend_->size();
    if (dxSize.isEmpty()) {
        backend_->releaseFrame();
        return colors;
    }
    colors.reserve(rasters.size());
    const double scaleX = double(dxSize.width()) / double(logicalSize.width());
    const double scaleY = double(dxSize.height()) / double(logicalSize.height());
    for (const ZoneRaster& raster : rasters) {
        if (raster.area.isEmpty() || raster.mask.isNull()) {
            colors.push_back(QColor());
            continue;
        }
        const QRect phys(qRound(raster.area.x() * scaleX), qRound(raster.area.y() * scaleY),
                         qRound(raster.area.width() * scaleX), qRound(raster.area.height() * scaleY));
        colors.push_back(backend_->sampleMask(phys, raster.mask));
    }
    backend_->releaseFrame();
    return colors;
}
#endif

QColor AmbiLight::combineZones(const QVector<QColor>& colors)
{
    if (colors.isEmpty()) return {};
    if (zoneLast_.size() != colors.size()) {
        zoneLast_ = QVector<QColor>(colors.size());
        zoneHasLast_ = QVector<bool>(colors.size(), false);
    }

    QVector<QColor> processed;
    QVector<double> weights;
    processed.reserve(colors.size());
    int index = 0;
    for (const CaptureZone& zone : zones_) {
        if (!zone.enabled || zone.isEmpty()) continue;
        if (index >= colors.size()) break;
        QColor color = colors.at(index);
        const int slot = index++;
        if (!color.isValid()) continue;

        const double zoneBoost = zone.boost < 0.0 ? boost_ : std::clamp(zone.boost, 1.0, 3.0);
        const double zoneSmooth = zone.smooth < 0.0 ? smooth_ : std::clamp(zone.smooth, 0.0, 0.9);
        color = boostColor(color, zoneBoost);
        color = adjustZoneColor(color, std::clamp(zone.brightness, 0.0, 2.0),
                                std::clamp(zone.saturation, 0.0, 2.0));
        // Кривые слоя применяем после яркости/насыщенности и до смешивания
        // слоёв, как это делает Photoshop для своих adjustment-слоёв.
        color = zone.applyCurves(color);
        if (zoneSmooth > 0.0 && zoneHasLast_[slot] && zoneLast_[slot].isValid()) {
            const QColor previous = zoneLast_[slot];
            color = QColor(int(previous.red() * zoneSmooth + color.red() * (1.0 - zoneSmooth)),
                           int(previous.green() * zoneSmooth + color.green() * (1.0 - zoneSmooth)),
                           int(previous.blue() * zoneSmooth + color.blue() * (1.0 - zoneSmooth)));
        }
        zoneLast_[slot] = color;
        zoneHasLast_[slot] = true;
        processed.push_back(color);
        weights.push_back(std::max(0.0, zone.weight));
    }
    if (processed.isEmpty()) return {};

    if (combine_ == "brightest" || combine_ == "saturated") {
        int best = 0;
        for (int i = 1; i < processed.size(); ++i) {
            const QColor& candidate = processed.at(i);
            const QColor& current = processed.at(best);
            const auto spread = [](const QColor& c) {
                return std::max({c.red(), c.green(), c.blue()}) - std::min({c.red(), c.green(), c.blue()});
            };
            const bool better = combine_ == "brightest"
                ? candidate.red() + candidate.green() + candidate.blue()
                      > current.red() + current.green() + current.blue()
                : spread(candidate) > spread(current);
            if (better) best = i;
        }
        return applyLevels(processed.at(best), minLevel_, autoBright_);
    }

    double total = 0.0;
    double red = 0.0, green = 0.0, blue = 0.0;
    for (int i = 0; i < processed.size(); ++i) {
        const double weight = weights.at(i);
        red += processed.at(i).red() * weight;
        green += processed.at(i).green() * weight;
        blue += processed.at(i).blue() * weight;
        total += weight;
    }
    if (total <= 0.0) return applyLevels(processed.constFirst(), minLevel_, autoBright_);
    return applyLevels(QColor(int(red / total), int(green / total), int(blue / total)),
                       minLevel_, autoBright_);
}

QColor AmbiLight::finishColor(const QColor& raw)
{
    if (!raw.isValid()) return {};
    QColor color = raw;
    int red = color.red(), green = color.green(), blue = color.blue();
    if (hasLast_) {
        red = clippedChannel(red, last_.red());
        green = clippedChannel(green, last_.green());
        blue = clippedChannel(blue, last_.blue());
        if (smooth_ > 0.0) {
            red = static_cast<int>(last_.red() * smooth_ + red * (1.0 - smooth_));
            green = static_cast<int>(last_.green() * smooth_ + green * (1.0 - smooth_));
            blue = static_cast<int>(last_.blue() * smooth_ + blue * (1.0 - smooth_));
        }
        if (std::max({qAbs(red - last_.red()), qAbs(green - last_.green()),
                      qAbs(blue - last_.blue())}) < 3)
            return {};
    }
    rawLast_ = raw;
    last_ = QColor(red, green, blue);
    hasLast_ = true;
    Q_EMIT colorChanged(red, green, blue);
    return last_;
}


void AmbiLight::capture()
{
    if (!screen_) selectScreen();
    if (!screen_) return;
#ifdef Q_OS_WIN
    if (backend_ && !backend_->active()
        && (!hwRetry_.isValid() || hwRetry_.elapsed() >= 3000)) {
        // После выхода из exclusive-fullscreen игры пробуем вернуть
        // аппаратный захват.
        hwRetry_.restart();
        selectScreen();
    }
#endif
    const QSize size = screen_->geometry().size();
    const bool useZones = hasZones();
    QColor raw;

    if (useZones) {
        rebuildZoneRasters(size);
        if (zoneRasters_.isEmpty()) return;
#ifdef Q_OS_WIN
        if (backend_ && backend_->active()) {
            const bool hasFrame = backend_->frameAvailable();
            const quint64 stillMs = hwStill_.isValid() ? quint64(hwStill_.elapsed()) : 1001;
            if (hasFrame) {
                hwStill_.restart();
                raw = combineZones(sampleZonesBackend(zoneRasters_, size));
            } else if (stillMs >= 1000) {
                // Статичный экран: аппаратные бэкенды (Desktop Duplication, WGC)
                // не выдают кадров без изменений, поэтому раз в секунду делаем
                // контрольный GDI-снимок, чтобы амбилайт не «замирал».
                hwStill_.restart();
                raw = combineZones(sampleZonesGdi(zoneRasters_));
            } else {
                return;
            }
        } else
#endif
        {
            raw = combineZones(sampleZonesGdi(zoneRasters_));
        }
    } else {
        const QVector<QRect> rects = buildRects(size.width(), size.height());
        QVector<QColor> samples;
#ifdef Q_OS_WIN
        if (backend_ && backend_->active()) {
            const bool hasFrame = backend_->frameAvailable();
            const quint64 stillMs = hwStill_.isValid() ? quint64(hwStill_.elapsed()) : 1001;
            if (hasFrame) {
                hwStill_.restart();
                samples = sampleBackend(rects, size);
            } else if (stillMs >= 1000) {
                hwStill_.restart();
                samples = sampleGdi(rects);
            } else {
                return;
            }
        } else
#endif
        {
            samples = sampleGdi(rects);
        }
        if (samples.isEmpty()) return;
        raw = processRaw(combine(samples));
    }

    finishColor(raw);
}

} // namespace elkbledom