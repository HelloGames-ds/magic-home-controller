#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QListWidget>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRectF>
#include <QTabWidget>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

#include "AmbiCanvas.h"
#include "CaptureZone.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QScrollArea;
class QSlider;
class QToolButton;
class QVBoxLayout;
QT_END_NAMESPACE

namespace elkbledom {

class AmbiLight;

// Редактор области захвата. Намеренно максимально простой: снимок экрана,
// готовые пресеты области и глубина полосы. Своих слоёв, рисования и истории
// здесь нет — сложные режимы добавим поэтапно.
// Наследуется именно QDialog: обычный QWidget в этом приложении рендерится
// полупрозрачным и не разворачивается по F11/кнопке заголовка.
class AmbiEditor : public QDialog
{
    Q_OBJECT

public:
    explicit AmbiEditor(AmbiLight* ambilight, QWidget* parent = nullptr);
    ~AmbiEditor() override;

    QVariantMap settings() const;
    // Актуальные слои для сохранения: сначала те, что нарисованы, иначе —
    // загруженные из настроек, чтобы пустой результат их не затирал.
    QString currentZones() const;
    void setSettings(const QVariantMap& values);
    // Задаёт кадр предпросмотра (снимок экрана, файл с диска).
    void setPreviewImage(const QPixmap& image);

Q_SIGNALS:
    void settingsApplied(const QVariantMap& values);
    // Сохранено для совместимости с главным окном: слои больше не редактируются,
    // но ранее сохранённые данные не теряются.
    void zonesChanged(const QString& zones);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    QVBoxLayout* buildUi();
    QWidget* buildSimplePage();
    QWidget* buildSimplePageContent();
    QWidget* buildAdvancedPage();
    QWidget* buildStatusBar(QWidget* parent);
    void buildAdvancedCanvasArea(QVBoxLayout* target);
    QWidget* buildLayersPanel(QWidget* parent);
    QWidget* buildLayerPropertiesPanel(QWidget* parent);
    QWidget* buildHistoryPanel(QWidget* parent);

    void grabScreenSnapshot();
    void loadImage();
    void clearImage();
    void refreshScreens();
    void syncControls();
    static bool combineModesValid(const QString& mode);
    void applySettings();
    void updateRegion();
    // Любое изменение свойств слоя: вес, усиление, яркость, насыщенность.
    void applyLayerProperty();
    void updateZoomLabel();
    void captureSelectedMonitor();

    void apply();
    void resetAll();
    void restoreDefaults();
    bool advancedMode() const;

    // Расширенный режим: слои.
    void refreshLayerList();
    void updateLayerInspector();
    void pushHistory(const QString& label);
    void stepHistory(int delta);
    void rebuildHistoryList();
    void collectZonesFromCanvas();
    void addLayer();
    void duplicateLayer();
    void deleteLayer();
    void setActiveLayer(int index);
    void setActiveTool(int index);
    void setDrawMode(int mode);
    void syncLayerColors();
    void applyEditorMode();

    AmbiLight* ambilight_ = nullptr;
    AmbiCanvas* canvas_ = nullptr;
    // Один и тот же холст показывается то в простом, то в расширенном режиме:
    // переносим его между двумя областями прокрутки при смене вкладки.
    QScrollArea* simpleView_ = nullptr;
    QScrollArea* advancedView_ = nullptr;
    QWidget* leftPanel_ = nullptr;
    QWidget* rightPanel_ = nullptr;

    QToolButton* grabButton_ = nullptr;
    QComboBox* screenCombo_ = nullptr;
    QComboBox* captureCombo_ = nullptr;
    QSlider* bandSlider_ = nullptr;
    QSlider* frequencySlider_ = nullptr;
    QSlider* boostSlider_ = nullptr;
    QSlider* smoothSlider_ = nullptr;
    QSlider* minLevelSlider_ = nullptr;
    QLabel* bandValue_ = nullptr;
    QLabel* frequencyValue_ = nullptr;
    QLabel* boostValue_ = nullptr;
    QLabel* smoothValue_ = nullptr;
    QLabel* minLevelValue_ = nullptr;
    QCheckBox* autoBrightCheck_ = nullptr;
    QComboBox* combineCombo_ = nullptr;
    QLabel* rawSwatch_ = nullptr;
    QLabel* finalSwatch_ = nullptr;
    // В расширенном режиме итоговый цвет тоже нужен, а образцы живут в
    // простой вкладке — без них Advanced выглядит «безцветным».
    QLabel* advRawSwatch_ = nullptr;
    QLabel* advFinalSwatch_ = nullptr;
    QLabel* advRawValueLabel_ = nullptr;
    QLabel* advFinalValueLabel_ = nullptr;
    // Вкладку переключал пользователь — режим слоёв тогда действительно меняется.
    bool modeTouched_ = false;
    // Кривые активного слоя.
    class CurveEditor* curveEditor_ = nullptr;
    QComboBox* curveChannel_ = nullptr;
    void loadCurveForActiveLayer();
    void updateLayerHistogram();
    QLabel* rawValueLabel_ = nullptr;
    QLabel* finalValueLabel_ = nullptr;
    QLabel* cursorLabel_ = nullptr;
    QLabel* zoomLabel_ = nullptr;
    QLabel* colorLabel_ = nullptr;
    QVector<QToolButton*> regionButtons_;

    // Расширенный режим.
    QTabWidget* modeTabs_ = nullptr;
    QVector<QToolButton*> toolButtons_;
    QVector<QPushButton*> modeButtons_;
    QComboBox* drawModeCombo_ = nullptr;
    QLabel* selectionSizeLabel_ = nullptr;
    QListWidget* layerList_ = nullptr;
    QToolButton* addLayerButton_ = nullptr;
    QToolButton* duplicateLayerButton_ = nullptr;
    QToolButton* deleteLayerButton_ = nullptr;
    QToolButton* undoButton_ = nullptr;
    QToolButton* redoButton_ = nullptr;
    QLineEdit* layerName_ = nullptr;
    QCheckBox* layerEnabled_ = nullptr;
    QCheckBox* layerSmoothGlobal_ = nullptr;
    QSlider* layerWeight_ = nullptr;
    QSlider* layerSmooth_ = nullptr;
    QSlider* layerBrightness_ = nullptr;
    QSlider* layerSaturation_ = nullptr;
    QLabel* layerWeightValue_ = nullptr;
    QLabel* layerSmoothValue_ = nullptr;
    QLabel* layerBrightnessValue_ = nullptr;
    QLabel* layerSaturationValue_ = nullptr;
    QListWidget* historyList_ = nullptr;
    bool historyExpanded_ = false;
    QVector<CaptureZone> zones_;
    int activeLayer_ = -1;
    QVector<QVector<CaptureZone>> history_;
    QVector<QString> historyLabels_;
    int historyIndex_ = -1;

    QPixmap pixmap_;
    QString storedZones_;
    QVariantMap appliedSettings_;

    QString region_ = QStringLiteral("full");
    int bandPct_ = 36;
    double frequency_ = 20.0;
    double boost_ = 1.3;      // насыщенность цвета
    double smooth_ = 0.3;     // сглаживание переходов
    int minLevel_ = 15;       // порог тёмного: ниже него цвет считается чёрным
    bool autoBright_ = true;  // автоматическая яркость
    QString combine_ = QStringLiteral("average");
    int screenIndex_ = 0;
    QString captureKey_ = QStringLiteral("auto");
    QRectF customRect_;

    bool updating_ = false;
    bool geometryRestored_ = false;
    bool maximizedOnce_ = false;
};

} // namespace elkbledom
