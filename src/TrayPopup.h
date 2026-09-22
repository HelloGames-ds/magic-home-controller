#pragma once

#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;
class QSlider;

namespace elkbledom {

class ColorWheel;

// Self-contained quick-settings popup. The owner supplies and consumes state
// exclusively through setters and signals; no MainWindow dependency is needed.
class TrayPopup final : public QWidget
{
    Q_OBJECT

public:
    explicit TrayPopup(QWidget* parent = nullptr);

    void syncState(int red, int green, int blue, int brightness,
                   const QString& mode, const QString& effect, int smoothingTauMs);
    void syncFromMain(int red, int green, int blue, int brightness,
                      const QString& mode, const QString& effect, int smoothingTauMs)
    {
        syncState(red, green, blue, brightness, mode, effect, smoothingTauMs);
    }

    QString mode() const;
    QString effect() const;

Q_SIGNALS:
    void colorChanged(int red, int green, int blue);
    void brightnessChanged(int value);
    void smoothingTauChanged(int milliseconds);
    void modeChanged(const QString& mode);
    void effectChanged(const QString& effect);
    void powerRequested(bool enabled);
    void settingsRequested();
    void windowRequested();

private:
    void updateModeControls();
    void updatePreview(int red, int green, int blue);

    ColorWheel* wheel_ = nullptr;
    QSlider* brightness_ = nullptr;
    QLabel* brightnessValue_ = nullptr;
    QSlider* tau_ = nullptr;
    QLabel* tauValue_ = nullptr;
    QLabel* previewDot_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QComboBox* effectCombo_ = nullptr;
};

} // namespace elkbledom
