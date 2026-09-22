#pragma once

#include "AmbiLight.h"

#include <QDialog>
#include <QPixmap>
#include <QRectF>
#include <QVariantMap>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;

namespace elkbledom {

class CanvasPreview final : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasPreview(QWidget* parent = nullptr);
    void setImage(const QPixmap& pixmap);
    void clearImage();
    void setRegion(const QString& region);
    void setBandPct(int percent);
    void setCustomRect(const QRectF& rect);
    void setPreviewData(const QVector<QRect>& rects, const QVector<QColor>& samples,
                        const QColor& combined);

Q_SIGNALS:
    void customRectChanged(double x, double y, double width, double height);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void updateCustom(const QPoint& first, const QPoint& second);
    QSize imageSize() const;

    QPixmap image_;
    QString region_ = QStringLiteral("top");
    int bandPct_ = 8;
    QRectF customRect_{0.0, 0.0, 1.0, 0.1};
    QVector<QRect> rects_;
    QVector<QColor> sampleColors_;
    QColor combined_{0, 0, 0};
    bool editMode_ = false;
    QPoint dragStart_;
    bool dragging_ = false;
    QRect imageRect_;
};

class AmbiEditor final : public QDialog
{
    Q_OBJECT
public:
    explicit AmbiEditor(AmbiLight* ambilight, QWidget* parent = nullptr);
    void setSettings(const QVariantMap& settings);
    QVariantMap settings() const;

Q_SIGNALS:
    void settingsApplied(const QVariantMap& settings);

private Q_SLOTS:
    void refreshScreens();
    void grabScreenSnapshot();
    void loadImage();
    void clearImage();
    void recompute();
    void resetDefaults();
    void applySettings();

private:
    void buildUi();
    void syncControls();

    AmbiLight* ambilight_ = nullptr;
    CanvasPreview* canvas_ = nullptr;
    QComboBox* screenCombo_ = nullptr;
    QComboBox* regionCombo_ = nullptr;
    QComboBox* combineCombo_ = nullptr;
    QComboBox* captureCombo_ = nullptr;
    QSlider* bandSlider_ = nullptr;
    QSlider* boostSlider_ = nullptr;
    QSlider* minSlider_ = nullptr;
    QSlider* smoothSlider_ = nullptr;
    QSlider* frequencySlider_ = nullptr;
    QCheckBox* autoBrightCheck_ = nullptr;
    QLabel* bandValue_ = nullptr;
    QLabel* boostValue_ = nullptr;
    QLabel* minValue_ = nullptr;
    QLabel* smoothValue_ = nullptr;
    QLabel* frequencyValue_ = nullptr;
    QLabel* sampledLabel_ = nullptr;
    QLabel* processedLabel_ = nullptr;

    QString region_ = QStringLiteral("top");
    int bandPct_ = 8;
    double boost_ = 1.3;
    double smooth_ = 0.3;
    int minLevel_ = 15;
    bool autoBright_ = false;
    double frequency_ = 3.0;
    int screenIndex_ = 0;
    QString combine_ = QStringLiteral("average");
    QString captureKey_ = QStringLiteral("auto");
    QRectF customRect_{0.0, 0.0, 1.0, 0.1};
    QPixmap pixmap_;
};

} // namespace elkbledom
