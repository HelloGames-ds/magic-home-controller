#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QVector>
#include <memory>

class QScreen;
class QTimer;

namespace elkbledom {

#ifdef Q_OS_WIN
class ScreenBackend;
#endif

// Режимы захвата экрана для Ambilight.
enum class CaptureMode {
    Auto, // DXGI, при сбое — GDI, со статик-восстановлением
    Dxgi, // Принудительно DXGI (при недоступности — GDI)
    Gdi,  // Принудительно GDI (BitBlt)
    Wgc   // Windows.Graphics.Capture (работает в true exclusive fullscreen)
};

// Захват экрана и преобразование семплов в цвет светодиодной ленты.
class AmbiLight final : public QObject
{
    Q_OBJECT

public:
    struct ScreenInfo {
        int index = 0;
        QString name;
        QSize size;
        bool primary = false;
    };

    explicit AmbiLight(QObject* parent = nullptr);
    ~AmbiLight() override;

    static const QHash<QString, QString>& regions();
    static const QHash<QString, QString>& combineModes();
    static QVector<ScreenInfo> listScreens();

    static QString captureModeName(CaptureMode mode);
    static CaptureMode captureModeFromString(const QString& key);
    static QString captureModeKey(CaptureMode mode);

    void setCaptureMode(CaptureMode mode);
    CaptureMode captureMode() const { return captureMode_; }

    void setRegion(const QString& region);
    void setBandPct(int percent);
    void setBoost(double boost);
    void setSmooth(double factor);
    void setMinLevel(int level);
    void setAutoBright(bool enabled);
    void setFrequency(double hz);
    void setScreenIndex(int index);
    void setCustomRect(double x, double y, double width, double height);
    void setCustomRect(const QRectF& rect);
    void setCombine(const QString& mode);

    QString region() const { return region_; }
    int bandPct() const { return bandPct_; }
    double boost() const { return boost_; }
    double smooth() const { return smooth_; }
    int minLevel() const { return minLevel_; }
    bool autoBright() const { return autoBright_; }
    double frequency() const { return frequency_; }
    int screenIndex() const { return screenIndex_; }
    QRectF customRect() const { return customRect_; }
    QString combineMode() const { return combine_; }
    bool isRunning() const;

    QVector<QRect> buildRects(int width, int height,
                              const QString& region = QString(),
                              int bandPct = -1,
                              const QRectF& customRect = QRectF()) const;
    QColor combine(const QVector<QColor>& samples,
                   const QString& mode = QString()) const;
    QColor combineSamples(const QVector<QColor>& samples,
                          const QString& mode = QString()) const;
    QColor processRaw(const QColor& raw, double boost = -1.0,
                      int minLevel = -1, int autoBright = -1) const;

public Q_SLOTS:
    void start();
    void stop();
    void capture();

Q_SIGNALS:
    void colorChanged(int red, int green, int blue);

private:
    void selectScreen();
    QVector<QColor> sampleGdi(const QVector<QRect>& rects) const;
#ifdef Q_OS_WIN
    QVector<QColor> sampleBackend(const QVector<QRect>& rects, const QSize& logicalSize) const;
#endif

    QTimer* timer_ = nullptr;
    QString region_ = QStringLiteral("top");
    int bandPct_ = 8;
    double boost_ = 1.3;
    double smooth_ = 0.3;
    int minLevel_ = 15;
    bool autoBright_ = false;
    double frequency_ = 1000.0 / 300.0;
    int screenIndex_ = 0;
    QScreen* screen_ = nullptr;
    QColor last_;
    QColor rawLast_;
    bool hasLast_ = false;
    QRectF customRect_{0.0, 0.0, 1.0, 0.1};
    QString combine_ = QStringLiteral("average");
    CaptureMode captureMode_ = CaptureMode::Auto;

#ifdef Q_OS_WIN
    std::unique_ptr<ScreenBackend> backend_;
    QElapsedTimer hwStill_;
    QElapsedTimer hwRetry_;
#endif
};

} // namespace elkbledom
