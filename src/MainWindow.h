#pragma once

#include "SettingsStore.h"

#include <QColor>
#include <QMainWindow>
#include <QVector>

class QAction;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QSystemTrayIcon;
class QTabWidget;
class QTimer;

namespace elkbledom {

class AmbiEditor;
class AmbiLight;
class ColorSmoother;
class ColorWheel;
class DeviceScanner;
class EffectEngine;
class LogWindow;
class SessionShutdownFilter;
class SettingsStore;
class TrayPopup;
class WifiManager;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QWidget* createControlTab();
    QWidget* createEffectsTab();
    QWidget* createAmbilightTab();
    QWidget* createSettingsTab();
    void connectSignals();
    void setupTray();
    void applySettingsToUi();
    void collectSettingsFromUi();
    void saveSettings();
    void setStatus(const QString& text);
    void setMode(const QString& mode, bool immediate = true);
    void setEffect(const QString& effect);
    void applyEffectState();
    void setPower(bool enabled);
    void setPowerOffOnShutdown(bool enabled);
    void sendShutdownPowerOff();
    void submitGeneratedColor(int red, int green, int blue);
    void onSmoothedColor(int red, int green, int blue);
    void setStaticColor(int red, int green, int blue);
    void updatePreview(const QColor& color);
    void updatePaletteUi();
    void selectPaletteSlot(int index);
    void pushEffectSettings();
    void pushAmbilightSettings();
    void refreshScreens();
    void updateAmbilightSummary();
    void runSmoothTest();
    void smoothTestStep();
    void showWindow();
    void showPopup();
    void refreshTrayIcon(bool force = false);
    void quitApplication();
    void openLogWindow();

    SettingsStore* store_ = nullptr;
    WifiManager* wifi_ = nullptr;
    EffectEngine* effects_ = nullptr;
    AmbiLight* ambilight_ = nullptr;
    ColorSmoother* smoother_ = nullptr;
    DeviceScanner* scanner_ = nullptr;
    TrayPopup* popup_ = nullptr;
    LogWindow* logWindow_ = nullptr;
    AmbiEditor* ambiEditor_ = nullptr;
    SessionShutdownFilter* shutdownFilter_ = nullptr;
    AppSettings settings_;

    QTabWidget* tabs_ = nullptr;
    QLabel* connectionDot_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    ColorWheel* colorWheel_ = nullptr;
    QLabel* colorPreview_ = nullptr;
    QLabel* hexLabel_ = nullptr;
    QSlider* brightnessSlider_ = nullptr;
    QLabel* brightnessValue_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QPushButton* smoothTestButton_ = nullptr;

    QComboBox* effectCombo_ = nullptr;
    QLabel* effectDescription_ = nullptr;
    QSlider* speedSlider_ = nullptr;
    QLabel* speedValue_ = nullptr;
    QSlider* intensitySlider_ = nullptr;
    QLabel* intensityValue_ = nullptr;
    QSlider* noiseSlider_ = nullptr;
    QLabel* noiseValue_ = nullptr;
    QCheckBox* reverseCheck_ = nullptr;
    ColorWheel* paletteWheel_ = nullptr;
    QLabel* paletteActiveLabel_ = nullptr;
    QLabel* paletteUsageLabel_ = nullptr;
    QVector<QPushButton*> paletteButtons_;
    QPushButton* palettePlus_ = nullptr;
    QPushButton* paletteMinus_ = nullptr;

    QComboBox* screenCombo_ = nullptr;
    QPushButton* screenRefreshButton_ = nullptr;
    QComboBox* ambiCaptureCombo_ = nullptr;
    QLabel* ambilightSummary_ = nullptr;
    QLabel* ambilightState_ = nullptr;

    QLineEdit* addressEdit_ = nullptr;
    QPushButton* applyAddressButton_ = nullptr;
    QPushButton* scanButton_ = nullptr;
    QListWidget* deviceList_ = nullptr;
    QPushButton* useDeviceButton_ = nullptr;
    QSlider* netIntervalSlider_ = nullptr;
    QLabel* netIntervalValue_ = nullptr;
    QSlider* netDedupSlider_ = nullptr;
    QLabel* netDedupValue_ = nullptr;
    QCheckBox* smoothingCheck_ = nullptr;
    QSlider* tauSlider_ = nullptr;
    QLabel* tauValue_ = nullptr;
    QCheckBox* loggingCheck_ = nullptr;
    QPushButton* openLogButton_ = nullptr;
    QPushButton* clearLogButton_ = nullptr;
    QCheckBox* autostartCheck_ = nullptr;
    QCheckBox* minimizedCheck_ = nullptr;
    QCheckBox* saveOnExitCheck_ = nullptr;
    QCheckBox* keepaliveCheck_ = nullptr;
    QCheckBox* restorePowerCheck_ = nullptr;
    QCheckBox* powerOffOnExitCheck_ = nullptr;
    QCheckBox* powerOffOnShutdownCheck_ = nullptr;
    QAction* powerOffOnShutdownAction_ = nullptr;
    QComboBox* languageCombo_ = nullptr;

    QSystemTrayIcon* tray_ = nullptr;
    QTimer* clickTimer_ = nullptr;
    QTimer* keepaliveTimer_ = nullptr;
    QTimer* iconTimer_ = nullptr;
    QTimer* smoothTestTimer_ = nullptr;
    QVector<QColor> smoothTestColors_;
    int smoothTestIndex_ = 0;
    QString smoothTestPrevMode_;
    QString smoothTestPrevEffect_;
    int paletteSlot_ = 0;
    QColor currentColor_{191, 0, 255};
    bool powerOn_ = true;
    bool updating_ = false;
    bool quitting_ = false;
    bool connected_ = false;
    QString trayIconKey_;
};

} // namespace elkbledom
