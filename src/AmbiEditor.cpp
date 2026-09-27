#include "AmbiEditor.h"
#include "AmbiCanvas.h"
#include "AmbiLight.h"
#include "AppLog.h"
#include "AmbiEditorWidgets.h"
#include "CaptureZone.h"
#include "CurveEditor.h"
#include "EditorIcons.h"
#include "config.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGui/QKeySequence>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <algorithm>
namespace elkbledom {
AmbiEditor::AmbiEditor(AmbiLight* ambilight, QWidget* parent)
    : QDialog(parent), ambilight_(ambilight)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setModal(false);
    // Обычное окно, а не диалоговое: у диалогов Windows не показывает кнопку
    // «Развернуть» и разворачивает их поверх панели задач.
    setWindowFlags(Qt::Window);
    // Явная непрозрачная заливка: без неё окно редактора просвечивает главное.
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(30, 30, 36));
    pal.setColor(QPalette::Base, QColor(35, 35, 43));
    setPalette(pal);
    setAutoFillBackground(true);
    setWindowTitle(tr("Ambilight capture area"));
    resize(1400, 900);
    // Как в исходной версии: стартовые значения берём из самого движка, а не из
    // локальных умолчаний. Тогда предпросмотр и лента считают одно и то же.
    if (ambilight_) {
        region_ = ambilight_->region();
        bandPct_ = ambilight_->bandPct();
        frequency_ = std::clamp(ambilight_->frequency(), 1.0, 30.0);
        boost_ = ambilight_->boost();
        smooth_ = ambilight_->smooth();
        minLevel_ = ambilight_->minLevel();
        autoBright_ = ambilight_->autoBright();
        combine_ = ambilight_->combineMode();
        screenIndex_ = ambilight_->screenIndex();
        customRect_ = ambilight_->customRect();
        captureKey_ = AmbiLight::captureModeKey(ambilight_->captureMode());
    }
    buildUi();
    syncControls();
    updateRegion();
    QTimer::singleShot(0, this, [this] { grabScreenSnapshot(); });
}
AmbiEditor::~AmbiEditor() = default;
QVBoxLayout* AmbiEditor::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);
    // Верхняя панель: снимок экрана, загрузка, масштаб.
    auto* header = new QHBoxLayout;
    header->setSpacing(6);
    auto* grab = actionButton(this, icons::Id::Capture, tr("Screenshot"),
                              tr("Take a screenshot of the selected monitor"));
    auto* load = actionButton(this, icons::Id::Open, tr("Load"), tr("Open an image from disk"));
    auto* clear = actionButton(this, icons::Id::Trash, tr("Clear"),
                               tr("Remove the image from the preview"));
    header->addWidget(grab);
    header->addWidget(load);
    header->addWidget(clear);
    // Кнопки действий были созданы, но ни к чему не подключены: снимок экрана,
    // загрузка и очистка не делали ничего.
    connect(grab, &QToolButton::clicked, this, &AmbiEditor::grabScreenSnapshot);
    connect(load, &QToolButton::clicked, this, &AmbiEditor::loadImage);
    connect(clear, &QToolButton::clicked, this, &AmbiEditor::clearImage);
    header->addSpacing(12);
    auto* zoomOut = bareIconButton(this, icons::Id::ZoomOut, tr("Zoom out"), false);
    auto* zoomIn = bareIconButton(this, icons::Id::ZoomIn, tr("Zoom in"), false);
    auto* fit = bareIconButton(this, icons::Id::Fit, tr("Fit the image in the window"), false);
    header->addWidget(zoomOut);
    header->addWidget(zoomIn);
    header->addWidget(fit);
    // Кнопки масштаба тоже остались без подключения: нажимались, но ничего
    // не делали.
    connect(zoomOut, &QToolButton::clicked, this, [this] {
        if (canvas_) canvas_->zoomBy(1.0 / 1.25);
    });
    connect(zoomIn, &QToolButton::clicked, this, [this] {
        if (canvas_) canvas_->zoomBy(1.25);
    });
    connect(fit, &QToolButton::clicked, this, [this] {
        if (canvas_) canvas_->zoomToFit();
    });
    header->addStretch();
    // Переключатель режимов отдельной строкой: внутри шапки он сжимал бы
    // содержимое страницы до пары пикселей.
    auto* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    if (auto* bar = tabs->findChild<QTabBar*>()) bar->setExpanding(false);
    modeTabs_ = tabs;
    root->addLayout(header);
    root->addWidget(tabs, 1);
    tabs->addTab(buildSimplePage(), tr("Simple"));
    tabs->addTab(buildAdvancedPage(), tr("Advanced"));
    tabs->setTabToolTip(0, tr("Only the ready-made capture areas"));
    tabs->setTabToolTip(1, tr("Draw and tune your own layers"));
    // Переключение вкладки пользователем меняет режим. Программная установка
    // вкладки при открытии — не меняет, иначе «Применить» в простой вкладке
    // молча выключал бы слои, нарисованные в расширенной.
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index >= 0) modeTouched_ = true;
        applyEditorMode();
    });
    // Горячие клавиши инструментов расширенного режима — как в Photoshop.
    struct ToolShortcut { QKeySequence key; AmbiCanvas::Tool tool; };
    const QVector<ToolShortcut> shortcuts{
        { QKeySequence(Qt::Key_M), AmbiCanvas::Tool::Rectangle },
        { QKeySequence(Qt::Key_V), AmbiCanvas::Tool::Move },
        { QKeySequence(Qt::Key_Z), AmbiCanvas::Tool::Zoom },
        { QKeySequence(Qt::Key_H), AmbiCanvas::Tool::Hand },
    };
    for (const ToolShortcut& shortcut : shortcuts) {
        auto* key = new QShortcut(shortcut.key, this);
        connect(key, &QShortcut::activated, this, [this, shortcut] {
            if (modeTabs_ && modeTabs_->currentIndex() != 1) modeTabs_->setCurrentIndex(1);
            setActiveTool(int(shortcut.tool));
        });
    }
    return root;
}
// Простой режим: превью и панель настроек справа.
void AmbiEditor::applyEditorMode()
{
    const bool advanced = modeTabs_ && modeTabs_->currentIndex() == 1;
    // Холст один на обе вкладки: переносим его в активную область прокрутки.
    QScrollArea* target = advanced ? advancedView_ : simpleView_;
    QScrollArea* source = advanced ? simpleView_ : advancedView_;
    if (canvas_ && target && target->widget() != canvas_) {
        // Именно takeWidget(), а не setWidget(nullptr): по документации Qt
        // setWidget удаляет прежний виджет, и холст исчезал бы совсем.
        if (source && source->widget() == canvas_) source->takeWidget();
        target->setWidget(canvas_);
        canvas_->show();
        // Вписываем с задержкой: на момент переключения область прокрутки ещё не
        // получила окончательный размер, и картинка вписалась бы не туда.
        QTimer::singleShot(0, this, [this] {
            canvas_->zoomToFit();
            updateZoomLabel();
        });
    }
    // В расширенном режиме рисуем слои, в простом — готовую область захвата.
    if (canvas_) {
        if (advanced) {
            canvas_->setZones(zones_);
            canvas_->setGlobalTuning(boost_, smooth_, autoBright_);
            syncLayerColors();
            canvas_->setActiveZone(activeLayer_);
        } else {
            canvas_->setZones({});
            canvas_->setActiveZone(-1);
        }
    }
    updateRegion();
    refreshLayerList();
    updateLayerInspector();
}
void AmbiEditor::setActiveTool(int tool)
{
    for (QToolButton* button : toolButtons_)
        button->setChecked(button->property("canvasTool").toInt() == tool);
    if (canvas_) canvas_->setTool(AmbiCanvas::Tool(tool));
}
void AmbiEditor::setDrawMode(int mode)
{
    if (canvas_) canvas_->setDrawMode(AmbiCanvas::DrawMode(mode));
    if (drawModeCombo_) {
        const QSignalBlocker blocker(drawModeCombo_);
        drawModeCombo_->setCurrentIndex(drawModeCombo_->findData(mode));
    }
}
// Цвет слоя по его номеру: одинаковые цвета делали слои неразличимыми.
static QColor layerPaletteColor(int index)
{
    static const QVector<QColor> palette{ QColor(176, 112, 255), QColor(96, 190, 255),
                                           QColor(255, 150, 120), QColor(120, 224, 160),
                                           QColor(255, 208, 96),  QColor(240, 128, 200),
                                           QColor(140, 160, 255), QColor(150, 224, 208) };
    return palette.at(std::abs(index) % palette.size());
}
void AmbiEditor::syncLayerColors()
{
    QVector<QColor> colors;
    colors.reserve(zones_.size());
    for (int i = 0; i < zones_.size(); ++i) {
        colors.append(zones_.at(i).enabled ? layerPaletteColor(i) : QColor(110, 110, 125));
    }
    if (canvas_) canvas_->setLayerColors(colors);
}
void AmbiEditor::collectZonesFromCanvas()
{
    if (canvas_) zones_ = canvas_->zones();
    syncLayerColors();
}
void AmbiEditor::stepHistory(int delta)
{
    const int next = historyIndex_ + delta;
    if (next < 0 || next >= history_.size()) return;
    historyIndex_ = next;
    zones_ = history_.at(next);
    if (activeLayer_ >= zones_.size()) activeLayer_ = zones_.isEmpty() ? -1 : zones_.size() - 1;
    syncLayerColors();
    if (canvas_) {
        canvas_->setZones(zones_);
        canvas_->setActiveZone(activeLayer_);
    }
    rebuildHistoryList();
    refreshLayerList();
    updateLayerInspector();
}
void AmbiEditor::refreshLayerList()
{
    if (!layerList_) return;
    const QSignalBlocker blocker(layerList_);
    layerList_->clear();
    for (int i = 0; i < zones_.size(); ++i) {
        const CaptureZone& zone = zones_.at(i);
        const QString title = zone.name.isEmpty() ? tr("Layer %1").arg(i + 1) : zone.name;
        auto* item = new QListWidgetItem(zone.enabled ? title : tr("%1 (off)").arg(title));
        QPixmap swatch(14, 14);
        swatch.fill(zone.enabled ? layerPaletteColor(i) : QColor(90, 90, 105));
        item->setIcon(QIcon(swatch));
        item->setToolTip(tr("%1 area(s), %2 cut-out(s)").arg(zone.areas.size()).arg(zone.masks.size()));
        if (!zone.enabled) item->setForeground(QColor(140, 140, 155));
        layerList_->addItem(item);
    }
    if (activeLayer_ >= 0 && activeLayer_ < layerList_->count())
        layerList_->setCurrentRow(activeLayer_);
    const bool has = activeLayer_ >= 0 && activeLayer_ < zones_.size();
    if (deleteLayerButton_) deleteLayerButton_->setEnabled(has);
    if (duplicateLayerButton_) duplicateLayerButton_->setEnabled(has);
    if (undoButton_) undoButton_->setEnabled(historyIndex_ > 0);
    if (redoButton_) redoButton_->setEnabled(historyIndex_ + 1 < history_.size());
}
void AmbiEditor::updateLayerInspector()
{
    const bool has = activeLayer_ >= 0 && activeLayer_ < zones_.size();
    const auto enable = [has](QWidget* widget) { if (widget) widget->setEnabled(has); };
    enable(layerName_);
    enable(layerEnabled_);
    enable(layerWeight_);
    enable(layerSmooth_);
    enable(layerSmoothGlobal_);
    enable(layerBrightness_);
    enable(layerSaturation_);
    if (!has) return;
    const CaptureZone& zone = zones_.at(activeLayer_);
    const QSignalBlocker nameBlock(layerName_);
    const QSignalBlocker enabledBlock(layerEnabled_);
    const QSignalBlocker weightBlock(layerWeight_);
    const QSignalBlocker smoothBlock(layerSmooth_);
    const QSignalBlocker smoothGlobalBlock(layerSmoothGlobal_);
    const QSignalBlocker brightBlock(layerBrightness_);
    const QSignalBlocker satBlock(layerSaturation_);
    layerName_->setText(zone.name);
    layerEnabled_->setChecked(zone.enabled);
    layerWeight_->setValue(qRound(zone.weight * 100));
    layerWeightValue_->setText(QString::number(zone.weight, 'f', 2));
    layerSmoothGlobal_->setChecked(zone.smooth < 0.0);
    layerSmooth_->setValue(qRound((zone.smooth < 0.0 ? smooth_ : zone.smooth) * 100));
    layerSmoothValue_->setText(QString::number(qRound((zone.smooth < 0.0 ? smooth_ : zone.smooth) * 100))
                               + QStringLiteral("%"));
    layerBrightness_->setValue(qRound(zone.brightness * 100));
    layerBrightnessValue_->setText(QString::number(zone.brightness, 'f', 2));
    layerSaturation_->setValue(qRound(zone.saturation * 100));
    layerSaturationValue_->setText(QString::number(zone.saturation, 'f', 2));
}
void AmbiEditor::pushHistory(const QString& label)
{
    if (historyIndex_ + 1 < history_.size()) {
        history_.resize(historyIndex_ + 1);
        historyLabels_.resize(historyIndex_ + 1);
    }
    history_.append(zones_);
    historyLabels_.append(label);
    while (history_.size() > 60) {
        history_.removeFirst();
        historyLabels_.removeFirst();
    }
    historyIndex_ = history_.size() - 1;
    rebuildHistoryList();
    refreshLayerList();
}
void AmbiEditor::rebuildHistoryList()
{
    if (!historyList_) return;
    const QSignalBlocker blocker(historyList_);
    historyList_->clear();
    for (int i = 0; i < historyLabels_.size(); ++i) {
        auto* item = new QListWidgetItem(historyLabels_.at(i));
        if (i > historyIndex_) item->setForeground(QColor(130, 130, 145));
        historyList_->addItem(item);
    }
    historyList_->setCurrentRow(historyIndex_);
}
void AmbiEditor::setActiveLayer(int index)
{
    activeLayer_ = (index >= 0 && index < zones_.size()) ? index : -1;
    if (canvas_) canvas_->setActiveZone(activeLayer_);
    refreshLayerList();
    updateLayerInspector();
}
void AmbiEditor::addLayer()
{
    CaptureZone zone;
    zone.name = tr("Layer %1").arg(zones_.size() + 1);
    zones_.append(zone);
    syncLayerColors();
    if (canvas_) canvas_->setZones(zones_);
    pushHistory(tr("Add layer"));
    // Именно setActiveLayer(): иначе холст продолжал рисовать старый слой,
    // и новый выглядел лишь выделенным в списке, но пустым.
    setActiveLayer(zones_.size() - 1);
    setActiveTool(int(AmbiCanvas::Tool::Rectangle));
    setDrawMode(int(AmbiCanvas::DrawMode::New));
}
void AmbiEditor::duplicateLayer()
{
    if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
    CaptureZone copy = zones_.at(activeLayer_);
    copy.name = tr("%1 (copy)").arg(copy.name);
    zones_.insert(activeLayer_ + 1, copy);
    const int copyIndex = activeLayer_ + 1;
    syncLayerColors();
    if (canvas_) canvas_->setZones(zones_);
    pushHistory(tr("Duplicate layer"));
    setActiveLayer(copyIndex);
}
void AmbiEditor::deleteLayer()
{
    if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
    zones_.removeAt(activeLayer_);
    activeLayer_ = zones_.isEmpty() ? -1 : std::min(activeLayer_, int(zones_.size()) - 1);
    syncLayerColors();
    if (canvas_) canvas_->setZones(zones_);
    pushHistory(tr("Delete layer"));
}
void AmbiEditor::updateZoomLabel()
{
    if (zoomLabel_)
        zoomLabel_->setText(tr("Zoom: %1%").arg(qRound(canvas_->zoom() * 100)));
}
void AmbiEditor::syncControls()
{
    if (updating_) return;
    updating_ = true;
    refreshScreens();
    if (bandSlider_) {
        bandSlider_->setValue(bandPct_);
        if (bandValue_) bandValue_->setText(QString::number(bandPct_) + QStringLiteral("%"));
    }
    if (frequencySlider_) {
        frequencySlider_->setValue(qRound(frequency_));
        if (frequencyValue_) frequencyValue_->setText(tr("%1 Hz").arg(qRound(frequency_)));
    }
    if (boostSlider_) {
        boostSlider_->setValue(qRound(boost_ * 100));
        if (boostValue_) boostValue_->setText(QString::number(boost_, 'f', 2) + QStringLiteral("x"));
    }
    if (smoothSlider_) {
        smoothSlider_->setValue(qRound(smooth_ * 100));
        if (smoothValue_) smoothValue_->setText(QString::number(qRound(smooth_ * 100))
                                                 + QStringLiteral("%"));
    }
    if (minLevelSlider_) {
        minLevelSlider_->setValue(minLevel_);
        if (minLevelValue_) minLevelValue_->setText(QString::number(minLevel_));
    }
    if (autoBrightCheck_) autoBrightCheck_->setChecked(autoBright_);
    if (combineCombo_ && combineModesValid(combine_)) {
        const int at = combineCombo_->findData(combine_);
        if (at >= 0) combineCombo_->setCurrentIndex(at);
    }
    if (captureCombo_) {
        const int at = captureCombo_->findData(captureKey_);
        if (at >= 0) captureCombo_->setCurrentIndex(at);
    }
    for (QToolButton* button : regionButtons_)
        button->setChecked(button->property("region").toString() == region_);
    updating_ = false;
}
bool AmbiEditor::combineModesValid(const QString& mode)
{
    return AmbiLight::combineModes().contains(mode);
}
void AmbiEditor::refreshScreens()
{
    if (!screenCombo_) return;
    const QSignalBlocker blocker(screenCombo_);
    const int previous = screenIndex_;
    screenCombo_->clear();
    for (const auto& screen : AmbiLight::listScreens()) {
        screenCombo_->addItem(QStringLiteral("%1 - %2 x %3%4")
                                  .arg(screen.name)
                                  .arg(screen.size.width())
                                  .arg(screen.size.height())
                                  .arg(screen.primary ? tr(" (primary)") : QString()),
                              screen.index);
    }
    const int at = screenCombo_->findData(previous);
    screenCombo_->setCurrentIndex(at >= 0 ? at : 0);
    screenIndex_ = screenCombo_->currentData().toInt();
}
void AmbiEditor::captureSelectedMonitor()
{
    const QVector<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty()) return;
    const int index = std::clamp(screenIndex_, 0, static_cast<int>(screens.size()) - 1);
    QPixmap shot = screens.at(index)->grabWindow(0);
    if (shot.isNull()) return;
    // Снимок уменьшаем: полноразмерный кадр только тормозит отрисовку.
    if (shot.width() > 1920) shot = shot.scaledToWidth(1920, Qt::SmoothTransformation);
    pixmap_ = shot;
    canvas_->setImage(shot);
    updateZoomLabel();
    updateRegion();
}
void AmbiEditor::grabScreenSnapshot()
{
    captureSelectedMonitor();
}
void AmbiEditor::setPreviewImage(const QPixmap& image)
{
    pixmap_ = image;
    if (canvas_) canvas_->setImage(image);
    updateZoomLabel();
    updateRegion();
}
void AmbiEditor::loadImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open an image"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (path.isEmpty()) return;
    QPixmap shot(path);
    if (shot.isNull()) return;
    if (shot.width() > 1920) shot = shot.scaledToWidth(1920, Qt::SmoothTransformation);
    pixmap_ = shot;
    canvas_->setImage(shot);
    updateZoomLabel();
    updateRegion();
}
void AmbiEditor::clearImage()
{
    pixmap_ = QPixmap();
    canvas_->setImage(QPixmap());
    updateZoomLabel();
    updateRegion();
}
// Цвет одного слоя: те же 9 точек, что и в основном редакторе, но по всем
// областям слоя и без вырезанных масок.
static QColor sampleLayerColor(const QImage& image, const CaptureZone& zone)
{
    qint64 rr = 0, gg = 0, bb = 0;
    qint64 n = 0;
    // Области слоя хранятся в нормализованных координатах (0..1), поэтому
    // переводим их в пиксели кадра.
    const QSizeF size(image.size());
    const auto toPixels = [size](const QPointF& at) {
        return QPointF(at.x() * size.width(), at.y() * size.height());
    };
    QVector<QPolygonF> masks;
    masks.reserve(zone.masks.size());
    for (const QPolygonF& mask : zone.masks) {
        QPolygonF moved;
        moved.reserve(mask.size());
        for (const QPointF& point : mask) moved.append(toPixels(point));
        masks.append(moved);
    }
    const auto samplePoint = [&](int x, int y) {
        if (!image.valid(x, y)) return;
        const QPointF at(x + 0.5, y + 0.5);
        for (const QPolygonF& mask : masks) {
            if (mask.containsPoint(at, Qt::OddEvenFill)) return;
        }
        const QColor c = image.pixelColor(x, y);
        rr += c.red();
        gg += c.green();
        bb += c.blue();
        ++n;
    };
    for (const QPolygonF& area : zone.areas) {
        const QRectF box = area.boundingRect();
        const QRect r = QRect(int(box.left() * size.width()), int(box.top() * size.height()),
                              qMax(1, int(box.width() * size.width())),
                              qMax(1, int(box.height() * size.height())))
                            .intersected(image.rect());
        if (r.isEmpty()) continue;
        for (int y : { r.center().y() - r.height() / 4, r.center().y(),
                       r.center().y() + r.height() / 4 }) {
            for (int x : { r.center().x() - r.width() / 4, r.center().x(),
                           r.center().x() + r.width() / 4 }) {
                samplePoint(x, y);
            }
        }
    }
    if (n == 0) return {};
    return QColor(int(rr / n), int(gg / n), int(bb / n));
}
// Общий цвет всех включённых слоёв: так же, как в движке, по весам.
static QColor combineLayerColors(const QVector<QColor>& colors, const QVector<CaptureZone>& zones)
{
    double wr = 0.0, wg = 0.0, wb = 0.0, wsum = 0.0;
    const int count = qMin(colors.size(), zones.size());
    for (int i = 0; i < count; ++i) {
        const CaptureZone& zone = zones.at(i);
        if (!zone.enabled || !colors.at(i).isValid()) continue;
        const double weight = std::max(0.0, zone.weight);
        if (weight <= 0.0) continue;
        wr += colors.at(i).red() * weight;
        wg += colors.at(i).green() * weight;
        wb += colors.at(i).blue() * weight;
        wsum += weight;
    }
    if (wsum <= 0.0) return {};
    return QColor(int(wr / wsum), int(wg / wsum), int(wb / wsum));
}
// Цвет слоя обрабатывается ровно теми же функциями, что и в движке, иначе
// предпросмотр показывал бы не то, что получит лента.
void AmbiEditor::loadCurveForActiveLayer()
{
    if (!curveEditor_) return;
    if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) {
        curveEditor_->setPoints({ QPointF(0.0, 0.0), QPointF(1.0, 1.0) });
        curveEditor_->setEnabled(false);
        return;
    }
    curveEditor_->setEnabled(true);
    const CaptureZone& zone = zones_.at(activeLayer_);
    const int channel = curveChannel_ ? curveChannel_->currentData().toInt() : 0;
    QVector<QPointF> points;
    switch (channel) {
    case 1: points = zone.curveR; break;
    case 2: points = zone.curveG; break;
    case 3: points = zone.curveB; break;
    default: points = zone.curveRgb; break;
    }
    curveEditor_->setPoints(points);
    static const QColor channelColors[4]{ QColor(235, 235, 240), QColor(255, 96, 96),
                                           QColor(96, 230, 120), QColor(110, 160, 255) };
    const int channelIndex = std::clamp(channel, 0, 3);
    curveEditor_->setChannelColor(channelColors[channelIndex]);
    curveEditor_->setChannel(channelIndex);
    updateLayerHistogram();
}
void AmbiEditor::updateLayerHistogram()
{
    if (!curveEditor_ || activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
    if (pixmap_.isNull()) return;
    const QImage image = pixmap_.toImage();
    const CaptureZone& zone = zones_.at(activeLayer_);
    QVector<int> red(256, 0), green(256, 0), blue(256, 0);
    const QSizeF size(image.size());
    // Считаем по сетке внутри областей слоя: гистограмма нужна для глаза,
    // точность здесь не важна.
    // Считаем по сетке внутри областей слоя: гистограмма нужна для глаза,
    // точность здесь не важна.
    for (const QPolygonF& area : zone.areas) {
        const QRectF box = area.boundingRect();
        for (int gy = 0; gy <= 48; ++gy) {
            for (int gx = 0; gx <= 48; ++gx) {
                const double fx = box.left() + box.width() * gx / 48.0;
                const double fy = box.top() + box.height() * gy / 48.0;
                if (!area.containsPoint(QPointF(fx, fy), Qt::OddEvenFill)) continue;
                const int x = std::clamp(int(fx * size.width()), 0, image.width() - 1);
                const int y = std::clamp(int(fy * size.height()), 0, image.height() - 1);
                if (!zone.masks.isEmpty()) {
                    const QPointF at(x + 0.5, y + 0.5);
                    bool cut = false;
                    for (const QPolygonF& mask : zone.masks) {
                        QPolygonF moved;
                        moved.reserve(mask.size());
                        for (const QPointF& p : mask)
                            moved.append(QPointF(p.x() * size.width(), p.y() * size.height()));
                        if (mask.containsPoint(at, Qt::OddEvenFill)) {
                            cut = true;
                            break;
                        }
                    }
                    if (cut) continue;
                }
                const QColor c = image.pixelColor(x, y);
                ++red[std::clamp(c.red(), 0, 255)];
                ++green[std::clamp(c.green(), 0, 255)];
                ++blue[std::clamp(c.blue(), 0, 255)];
            }
        }
    }
    curveEditor_->setHistogram(red, green, blue);
}
void AmbiEditor::applyLayerProperty()
{
    if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
    if (canvas_) {
        canvas_->setZones(zones_);
        canvas_->setActiveZone(activeLayer_);
    }
    if (ambilight_) ambilight_->setZones(zones_);
    refreshLayerList();
    // Без этого образцы итогового цвета не менялись до переоткрытия редактора.
    updateRegion();
}
void AmbiEditor::updateRegion()
{
    for (QToolButton* button : regionButtons_)
        button->setChecked(button->property("region").toString() == region_);
    if (!ambilight_) return;
    ambilight_->setRegion(region_);
    ambilight_->setBandPct(bandPct_);
    ambilight_->setFrequency(frequency_);
    ambilight_->setBoost(boost_);
    ambilight_->setSmooth(smooth_);
    ambilight_->setMinLevel(minLevel_);
    ambilight_->setAutoBright(autoBright_);
    ambilight_->setCombine(combine_);
    ambilight_->setCustomRect(customRect_);
    canvas_->setRegion(region_);
    canvas_->setBandPct(bandPct_);
    canvas_->setCustomRect(customRect_);
    if (pixmap_.isNull() || !canvas_) return;
    // Расчёт цвета взят из исходной версии один в один: 9 точек вокруг центра
    // каждой области, прямоугольник обрезается по кадру. Эта схема совпадает с
    // тем, как AmbiLight считает цвет для ленты.
    const QImage image = pixmap_.toImage();
    const bool advanced = advancedMode();
    QVector<QRect> rects;
    QVector<QColor> colors;
    if (!advanced) {
        rects = ambilight_->buildRects(image.width(), image.height(), region_, bandPct_,
                                        customRect_);
    }
    for (QRect r : rects) {
        r = r.intersected(image.rect());
        if (r.isEmpty()) continue;
        qint64 rr = 0, gg = 0, bb = 0, n = 0;
        const int cx = r.center().x(), cy = r.center().y();
        for (int y : { cy - r.height() / 4, cy, cy + r.height() / 4 }) {
            for (int x : { cx - r.width() / 4, cx, cx + r.width() / 4 }) {
                if (!image.valid(x, y)) continue;
                const QColor c = image.pixelColor(x, y);
                rr += c.red();
                gg += c.green();
                bb += c.blue();
                ++n;
            }
        }
        if (n) colors.push_back(QColor(int(rr / n), int(gg / n), int(bb / n)));
    }
    if (colors.isEmpty() && !advanced) {
        const QString empty = tr("no data");
        if (rawValueLabel_) rawValueLabel_->setText(empty);
        if (finalValueLabel_) finalValueLabel_->setText(empty);
        if (advRawValueLabel_) advRawValueLabel_->setText(empty);
        if (advFinalValueLabel_) advFinalValueLabel_->setText(empty);
        if (colorLabel_) colorLabel_->setText(empty);
        return;
    }
    // В расширенном режиме считаем по слоям: сначала свой цвет каждого слоя,
    // затем общий по весам. Область основного редактора тут ни при чём — из-за
    // неё цвет «заседал» и не менялся при правке слоёв.
    QColor average;
    QColor combined;
    if (advanced) {
        ambilight_->setZones(zones_);
        QVector<QRect> layerRects;
        for (int i = 0; i < zones_.size(); ++i) {
            const CaptureZone& zone = zones_.at(i);
            QColor own = sampleLayerColor(image, zone);
            if (own.isValid()) {
                const double zoneBoost = zone.boost < 0.0 ? boost_ : std::clamp(zone.boost, 1.0, 3.0);
            own = AmbiLight::applyBoost(own, zoneBoost);
            own = AmbiLight::applyBrightnessSaturation(own, std::clamp(zone.brightness, 0.0, 2.0),
                                                       std::clamp(zone.saturation, 0.0, 2.0));
                own = zone.applyCurves(own);
            }
            colors.append(own);
            for (const QPolygonF& area : zone.areas) {
                const QRectF box = area.boundingRect();
                layerRects.append(
                    QRect(int(box.left() * image.width()), int(box.top() * image.height()),
                          qMax(1, int(box.width() * image.width())),
                          qMax(1, int(box.height() * image.height()))));
            }
        }
        rects = layerRects;
        // Образец слева — цвет выбранного слоя, справа — общий по всем слоям.
        average = (activeLayer_ >= 0 && activeLayer_ < colors.size()) ? colors.at(activeLayer_)
                                                                       : QColor();
        if (!average.isValid()) {
            for (const QColor& c : colors) {
                if (c.isValid()) {
                    average = c;
                    break;
                }
            }
        }
        combined = ambilight_
            ? ambilight_->processRaw(combineLayerColors(colors, zones_), boost_, minLevel_,
                                     autoBright_ ? 1 : 0)
            : QColor();
    } else {
        average = ambilight_ ? ambilight_->combine(colors, QStringLiteral("average")) : QColor();
        combined = ambilight_
            ? ambilight_->processRaw(ambilight_->combine(colors, combine_), boost_, minLevel_,
                                     autoBright_ ? 1 : 0)
            : QColor();
    }
    canvas_->setRegionSamples(rects, colors, combined);
    // Пишем значения в лог: по нему видно, отличается ли цвет в предпросмотре от
    // цвета на ленте, не нужно гадать по квадратику.
    AppLog::logline(QStringLiteral("editor preview: areas=%1 samples=%2 average=%3 strip=%4 "
                                   "boost=%5 min=%6 autoBright=%7 mode=%8")
                        .arg(rects.size())
                        .arg(colors.size())
                        .arg(average.isValid() ? average.name(QColor::HexRgb) : QStringLiteral("-"))
                        .arg(combined.isValid() ? combined.name(QColor::HexRgb) : QStringLiteral("-"))
                        .arg(boost_, 0, 'f', 2)
                        .arg(minLevel_)
                        .arg(autoBright_ ? 1 : 0)
                        .arg(combine_));
    const auto showSwatch = [](QLabel* chip, QLabel* valueLabel, const QColor& color) {
        if (chip) {
            chip->setStyleSheet(
                QStringLiteral("background:%1;border:1px solid #6a6a78;border-radius:6px")
                    .arg(color.isValid() ? color.name(QColor::HexRgb) : QStringLiteral("#23232b")));
        }
        if (valueLabel)
            valueLabel->setText(color.isValid() ? color.name(QColor::HexRgb).toUpper()
                                                : QStringLiteral("-"));
    };
    showSwatch(rawSwatch_, rawValueLabel_, average);
    showSwatch(finalSwatch_, finalValueLabel_, combined);
    showSwatch(advRawSwatch_, advRawValueLabel_, average);
    showSwatch(advFinalSwatch_, advFinalValueLabel_, combined);
    if (colorLabel_) {
        colorLabel_->setText(tr("Average: %1   strip: %2")
                                 .arg(average.isValid() ? average.name(QColor::HexRgb)
                                                        : tr("no data"),
                                      combined.isValid() ? combined.name(QColor::HexRgb)
                                                        : tr("no data")));
    }
}
void AmbiEditor::apply()
{
    appliedSettings_ = settings();
    Q_EMIT settingsApplied(appliedSettings_);
    // Раньше здесь отправлялся storedZones_ — значение, загруженное при
    // открытии редактора. Главное окно принимало его и перезаписывало только
    // что применённые слои на старые, поэтому эффект слетал сразу после
    // закрытия редактора.
    Q_EMIT zonesChanged(currentZones());
}
void AmbiEditor::resetAll()
{
    restoreDefaults();
    apply();
}
void AmbiEditor::restoreDefaults()
{
    // Как в исходной версии: сброс к значениям по умолчанию.
    updating_ = true;
    region_ = QStringLiteral("top");
    bandPct_ = 8;
    boost_ = 1.3;
    smooth_ = 0.3;
    minLevel_ = 15;
    autoBright_ = false;
    frequency_ = 3.0;
    combine_ = QStringLiteral("average");
    customRect_ = QRectF(0.0, 0.0, 1.0, 0.1);
    captureKey_ = QStringLiteral("auto");
    syncControls();
    updating_ = false;
    updateRegion();
}
QString AmbiEditor::currentZones() const
{
    // Актуальные слои; если их ещё нет, сохраняем те, что пришли из настроек,
    // чтобы переключение вкладок ничего не затирало.
    return zones_.isEmpty() ? storedZones_ : encodeZones(zones_);
}
QVariantMap AmbiEditor::settings() const
{
    return { { QStringLiteral("ambi_region"), region_ },
             { QStringLiteral("ambi_band"), bandPct_ },
             { QStringLiteral("ambi_boost"), boost_ },
             { QStringLiteral("ambi_smooth"), smooth_ },
             { QStringLiteral("ambi_min"), minLevel_ },
             { QStringLiteral("ambi_auto"), autoBright_ },
             { QStringLiteral("ambi_freq"), frequency_ },
             { QStringLiteral("ambi_screen"), screenIndex_ },
             { QStringLiteral("ambi_combine"), combine_ },
             { QStringLiteral("ambi_capture"), captureKey_ },
             { QStringLiteral("ambi_rect"), customRect_ },
             // Если вкладку не трогали, сохраняем прежний режим: иначе одно
             // нажатие «Применить» в простой вкладке тихо отключало бы слои.
             { QStringLiteral("ambi_easy"),
               modeTouched_ ? !advancedMode()
                            : appliedSettings_.value(QStringLiteral("ambi_easy"), true).toBool() },
             // Слои не стираем при переключении на простой режим: одно нажатие
             // «Применить» в простой вкладке уничтожало бы все слои. Движок их
             // и так игнорирует, пока ambi_easy=true.
             { QStringLiteral("ambi_zones"), currentZones() } };
}
bool AmbiEditor::advancedMode() const
{
    return modeTabs_ && modeTabs_->currentIndex() == 1;
}
void AmbiEditor::setSettings(const QVariantMap& values)
{
    updating_ = true;
    region_ = values.value(QStringLiteral("ambi_region"), region_).toString();
    bandPct_ = values.value(QStringLiteral("ambi_band"), bandPct_).toInt();
    frequency_ = std::clamp(values.value(QStringLiteral("ambi_freq"), frequency_).toDouble(), 1.0, 30.0);
    boost_ = std::clamp(values.value(QStringLiteral("ambi_boost"), boost_).toDouble(), 1.0, 3.0);
    smooth_ = std::clamp(values.value(QStringLiteral("ambi_smooth"), smooth_).toDouble(), 0.0, 0.9);
    minLevel_ = std::clamp(values.value(QStringLiteral("ambi_min"), minLevel_).toInt(), 0, 120);
    autoBright_ = values.value(QStringLiteral("ambi_auto"), autoBright_).toBool();
    combine_ = values.value(QStringLiteral("ambi_combine"), combine_).toString();
    screenIndex_ = values.value(QStringLiteral("ambi_screen"), screenIndex_).toInt();
    captureKey_ = values.value(QStringLiteral("ambi_capture"), captureKey_).toString();
    customRect_ = values.value(QStringLiteral("ambi_rect"), customRect_).toRectF();
    storedZones_ = values.value(QStringLiteral("ambi_zones")).toString();
    zones_ = decodeZones(storedZones_);
    activeLayer_ = zones_.isEmpty() ? -1 : 0;
    // По умолчанию открывается простая вкладка: она и есть «обычный» режим.
    // Переключаться на расширенную пользователь может сам, а режим, при котором
    // действуют слои, задаётся переключателем.
    if (modeTabs_) modeTabs_->setCurrentIndex(0);
    appliedSettings_ = values;
    updating_ = false;
    syncControls();
    applyEditorMode();
    updateRegion();
}
void AmbiEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (!geometryRestored_) {
        QSettings store = viewStore();
        const QByteArray geometry = store.value(QStringLiteral("editor/geometry")).toByteArray();
        // Сохранённую геометрию восстанавливаем только если она осмысленная:
        // иначе окно может открыться «за» верхним краем экрана без панели.
        if (!geometry.isEmpty()) {
            restoreGeometry(geometry);
            if (frameGeometry().width() < 400 || frameGeometry().height() < 300) {
                resize(1400, 900);
                if (QScreen* target = screen()) move(target->availableGeometry().topLeft());
            }
        }
        geometryRestored_ = true;
    }
    if (!maximizedOnce_) {
        maximizedOnce_ = true;
        QTimer::singleShot(0, this, [this] { showMaximized(); });
    }
    if (pixmap_.isNull()) grabScreenSnapshot();
}
void AmbiEditor::closeEvent(QCloseEvent* event)
{
    QSettings store = viewStore();
    store.setValue(QStringLiteral("editor/geometry"), saveGeometry());
    QDialog::closeEvent(event);
}
} // namespace elkbledom
