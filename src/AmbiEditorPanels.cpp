#include "AmbiEditorWidgets.h"

#include "AmbiCanvas.h"
#include "AmbiEditor.h"
#include "AmbiLight.h"
#include "CaptureZone.h"
#include "CurveEditor.h"
#include "EditorIcons.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace elkbledom {

// Построение интерфейса редактора. Логика осталась в AmbiEditor.cpp, вёрстка
// вынесена сюда: иначе файл нечитаем.

QWidget* AmbiEditor::buildSimplePage()
{
    auto* page = new QWidget;
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    auto* center = new QWidget(page);
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(6);
    auto* view = new QScrollArea(center);
    view->setWidgetResizable(false);
    view->setAlignment(Qt::AlignCenter);
    view->setFrameShape(QFrame::NoFrame);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    simpleView_ = view;
    if (!canvas_) canvas_ = new AmbiCanvas(view);
    view->setWidget(canvas_);
    connect(canvas_, &AmbiCanvas::cursorMoved, this, [this](int x, int y) {
        if (cursorLabel_) cursorLabel_->setText(tr("Cursor: %1, %2").arg(x).arg(y));
    });
    connect(canvas_, &AmbiCanvas::viewChanged, this, [this] { updateZoomLabel(); });
    centerLayout->addWidget(view, 1);
    centerLayout->addWidget(buildStatusBar(center));
    row->addWidget(center, 1);
    auto* right = new QWidget(page);
    rightPanel_ = right;
    right->setMinimumWidth(470);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(12, 0, 0, 0);
    rightLayout->addWidget(buildSimplePageContent());
    row->addWidget(right);
    return page;
}
// Расширенный режим: инструменты, превью, слои и свойства.
QWidget* AmbiEditor::buildAdvancedPage()
{
    auto* page = new QWidget;
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    auto* center = new QWidget(page);
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(6);
    buildAdvancedCanvasArea(centerLayout);
    row->addWidget(center, 1);
    auto* right = new QWidget(page);
    right->setMinimumWidth(340);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(12, 0, 0, 0);
    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(10);
    bodyLayout->addWidget(buildLayersPanel(body));
    bodyLayout->addWidget(buildLayerPropertiesPanel(body));
    bodyLayout->addWidget(buildHistoryPanel(body));
    bodyLayout->addStretch();
    scroll->setWidget(body);
    rightLayout->addWidget(scroll);
    // Кнопки применения в расширенном режиме: без них цвет и слои нельзя было
    // отдать ленте из этой вкладки. Те же действия, что и в простой.
    auto* actions = new QHBoxLayout;
    actions->setSpacing(6);
    auto* reset = new QPushButton(tr("Reset"), right);
    reset->setToolTip(tr("Return the capture area settings to the defaults"));
    auto* cancel = new QPushButton(tr("Cancel"), right);
    auto* apply = new QPushButton(tr("Apply"), right);
    auto* ok = new QPushButton(tr("OK"), right);
    ok->setDefault(true);
    actions->addWidget(reset);
    actions->addStretch();
    actions->addWidget(cancel);
    actions->addWidget(apply);
    actions->addWidget(ok);
    rightLayout->addLayout(actions);
    connect(reset, &QPushButton::clicked, this, &AmbiEditor::resetAll);
    connect(cancel, &QPushButton::clicked, this, &QWidget::close);
    connect(apply, &QPushButton::clicked, this, &AmbiEditor::apply);
    connect(ok, &QPushButton::clicked, this, [this] { AmbiEditor::apply(); close(); });
    row->addWidget(right);
    return page;
}
void AmbiEditor::buildAdvancedCanvasArea(QVBoxLayout* target)
{
    // Верхняя панель выделения: режим прямоугольника и размер.
    auto* options = new QHBoxLayout;
    options->setSpacing(6);
    auto* modeLabel = new QLabel(tr("Rectangle:"), this);
    drawModeCombo_ = new QComboBox(this);
    drawModeCombo_->addItem(tr("New"), int(AmbiCanvas::DrawMode::New));
    drawModeCombo_->addItem(tr("Add"), int(AmbiCanvas::DrawMode::Add));
    drawModeCombo_->addItem(tr("Subtract"), int(AmbiCanvas::DrawMode::Subtract));
    drawModeCombo_->setToolTip(tr("New replaces the selection, Add extends it, Subtract cuts it out"));
    selectionSizeLabel_ = new QLabel(QStringLiteral("0 x 0 px"), this);
    selectionSizeLabel_->setProperty("role", "status");
    options->addWidget(modeLabel);
    options->addWidget(drawModeCombo_);
    options->addSpacing(12);
    options->addWidget(selectionSizeLabel_);
    options->addStretch();
    target->addLayout(options);
    auto* body = new QHBoxLayout;
    body->setSpacing(0);
    // Вертикальная панель инструментов, как в Photoshop.
    auto* strip = new QWidget(this);
    strip->setFixedWidth(44);
    // Кнопки инструментов: одинаковый размер, тонкая рамка вместо крупной
    // заливки — иначе активная кнопка выглядит вдвое крупнее остальных.
    strip->setStyleSheet(QStringLiteral(
        "QToolButton{background:transparent;border:1px solid transparent;border-radius:8px;"
        "padding:0px;}"
        "QToolButton:hover{background:#2f2f3c;border:1px solid #44444f;}"
        "QToolButton:checked{background:#33334a;border:1px solid #7b6bd6;}"));
    auto* stripLayout = new QVBoxLayout(strip);
    stripLayout->setContentsMargins(4, 4, 4, 4);
    stripLayout->setSpacing(6);
    const QVector<AmbiCanvas::Tool> tools{ AmbiCanvas::Tool::Rectangle, AmbiCanvas::Tool::Move,
                                           AmbiCanvas::Tool::Zoom, AmbiCanvas::Tool::Hand };
    for (AmbiCanvas::Tool tool : tools) {
        auto* button = new QToolButton(strip);
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setFixedSize(34, 34);
        button->setIconSize(QSize(20, 20));
        button->setFocusPolicy(Qt::NoFocus);
        button->setProperty("tool", true);
        button->setProperty("canvasTool", int(tool));
        button->setIcon(icons::make(toolIcons(tool)));
        button->setToolTip(toolToolTip(tool));
        stripLayout->addWidget(button);
        toolButtons_.append(button);
        connect(button, &QToolButton::clicked, this, [this, tool] { setActiveTool(int(tool)); });
    }
    stripLayout->addStretch();
    body->addWidget(strip);
    // Итоговый цвет нужен в обеих вкладках: в простой он уже есть ниже,
    // здесь образцы были только в простой, и Advanced выглядел пустым.
    auto* advColorRow = new QHBoxLayout;
    advColorRow->setSpacing(8);
    advColorRow->addWidget(new QLabel(tr("Resulting color"), this));
    advColorRow->addSpacing(8);
    advRawSwatch_ = new QLabel(this);
    advRawSwatch_->setFixedSize(30, 20);
    advRawSwatch_->setFrameShape(QFrame::Box);
    advRawValueLabel_ = new QLabel(QStringLiteral("-"), this);
    advRawValueLabel_->setProperty("role", "hex");
    advFinalSwatch_ = new QLabel(this);
    advFinalSwatch_->setFixedSize(30, 20);
    advFinalSwatch_->setFrameShape(QFrame::Box);
    advFinalValueLabel_ = new QLabel(QStringLiteral("-"), this);
    advFinalValueLabel_->setProperty("role", "hex");
    advColorRow->addWidget(advRawSwatch_);
    advColorRow->addWidget(advRawValueLabel_);
    advColorRow->addSpacing(8);
    advColorRow->addWidget(advFinalSwatch_);
    advColorRow->addWidget(advFinalValueLabel_);
    advColorRow->addStretch();
    // Именно в центральную колонку под холстом: в горизонтальный ряд с
    // панелью инструментов строка попадала поверх картинки.
    target->addLayout(advColorRow);
    auto* right = new QVBoxLayout;
    right->setSpacing(6);
    auto* view = new QScrollArea(this);
    view->setWidgetResizable(false);
    view->setAlignment(Qt::AlignCenter);
    view->setFrameShape(QFrame::NoFrame);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    advancedView_ = view;
    right->addWidget(view, 1);
    right->addWidget(buildStatusBar(this));
    body->addLayout(right, 1);
    target->addLayout(body, 1);
    connect(drawModeCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        setDrawMode(drawModeCombo_->currentData().toInt());
    });
    connect(canvas_, &AmbiCanvas::selectionSizeChanged, this, [this](int w, int h) {
        if (selectionSizeLabel_) selectionSizeLabel_->setText(tr("%1 x %2 px").arg(w).arg(h));
    });
    connect(canvas_, &AmbiCanvas::zonesChanged, this, [this] {
        collectZonesFromCanvas();
        pushHistory(tr("Selection"));
        // Без пересчёта образец итогового цвета оставался «засевшим»:
        // правка слоёв не меняла показанный цвет.
        updateRegion();
        // Гистограмма слоя должна следовать за его геометрией, а не только за
        // свойствами: раньше она обновлялась лишь при смене слоя или канала.
        updateLayerHistogram();
    });
    connect(canvas_, &AmbiCanvas::activeZoneChanged, this, [this](int index) {
        activeLayer_ = index;
        refreshLayerList();
        updateLayerInspector();
    });
}
QWidget* AmbiEditor::buildSimplePageContent()
{
    auto* page = new QWidget;
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    auto* scroll = new QScrollArea(page);
    pageLayout->addWidget(scroll);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 8, 14);
    layout->setSpacing(10);
    scroll->setWidget(body);
    // --- Заголовок панели ---
    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(6);
    auto* title = new QLabel(tr("Capture area"), body);
    title->setProperty("role", "paneltitle");
    auto* help = bareIconButton(body, icons::Id::PanelRight,
                                tr("What is captured and how the colors are combined"), false);
    help->setFixedSize(22, 22);
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(help);
    layout->addLayout(titleRow);
    // --- 1. Тип области ---
    layout->addWidget(sectionTitle(tr("1. Area type"), body));
    auto* grid = new QGridLayout;
    grid->setSpacing(6);
    const QStringList order{ "top",  "bottom", "left",      "right", "center",
                             "full", "grid_3x3", "grid_5x3", "corners" };
    for (int i = 0; i < order.size(); ++i) {
        const QString key = order.at(i);
        const QString name = AmbiLight::regions().value(key, key);
        auto* button = new QToolButton(body);
        button->setText(name);
        button->setProperty("preset", true);
        button->setProperty("region", key);
        button->setCheckable(true);
        button->setToolTip(name);
        button->setIcon(regionIcon(key, QColor(200, 200, 215)));
        button->setIconSize(QSize(26, 18));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setFocusPolicy(Qt::NoFocus);
        // Кнопка не должна задавать минимальную ширину панели.
        button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        button->setMinimumHeight(62);
        regionButtons_.append(button);
        grid->addWidget(button, i / 3, i % 3);
        grid->setColumnStretch(i % 3, 1);
        connect(button, &QToolButton::clicked, this, [this, key] {
            region_ = key;
            updateRegion();
        });
    }
    layout->addLayout(grid);
    // --- 2. Параметры полосы ---
    layout->addWidget(sectionTitle(tr("2. Band settings"), body));
    layout->addLayout(settingRow(body, tr("Band depth"), tr("How deep the band reaches into the screen"),
                                 bandSlider_, bandValue_, 2, 90, bandPct_));
    bandValue_->setText(QString::number(bandPct_) + QStringLiteral("%"));
    layout->addLayout(settingRow(body, tr("Capture frequency"),
                                 tr("How many times per second the screen is sampled"),
                                 frequencySlider_, frequencyValue_, 1, 30, qRound(frequency_)));
    frequencyValue_->setText(tr("%1 Hz").arg(qRound(frequency_)));
    // --- 3. Цвет и обработка ---
    layout->addWidget(sectionTitle(tr("3. Color and processing"), body));
    layout->addLayout(settingRow(body, tr("Saturation"), tr("Color saturation of the strip"),
                                 boostSlider_, boostValue_, 100, 300, qRound(boost_ * 100)));
    boostValue_->setText(QString::number(boost_, 'f', 2) + QStringLiteral("x"));
    layout->addLayout(settingRow(body, tr("Dark threshold"), tr("Darker colors are turned off"),
                                 minLevelSlider_, minLevelValue_, 0, 120, minLevel_));
    minLevelValue_->setText(QString::number(minLevel_));
    layout->addLayout(settingRow(body, tr("Transition speed"), tr("How fast the strip changes color"),
                                 smoothSlider_, smoothValue_, 0, 90, qRound(smooth_ * 100)));
    smoothValue_->setText(QString::number(qRound(smooth_ * 100)) + QStringLiteral("%"));
    auto* autoRow = new QHBoxLayout;
    auto* autoLabel = new QLabel(tr("Auto brightness"), body);
    autoLabel->setToolTip(tr("Stretch the color to full brightness"));
    autoBrightCheck_ = new QCheckBox(body);
    autoBrightCheck_->setToolTip(tr("Stretch the color to full brightness"));
    autoBrightCheck_->setChecked(autoBright_);
    autoRow->addWidget(autoLabel);
    autoRow->addStretch();
    autoRow->addWidget(autoBrightCheck_);
    layout->addLayout(autoRow);
    auto* modeRow = new QHBoxLayout;
    modeRow->setSpacing(8);
    auto* modeLabel = new QLabel(tr("Mode:"), body);
    modeLabel->setToolTip(tr("How the colors of the capture areas become one color"));
    combineCombo_ = new QComboBox(body);
    combineCombo_->setToolTip(tr("How the colors of the capture areas become one color"));
    for (const QString& key : { QStringLiteral("average"), QStringLiteral("saturated"),
                                QStringLiteral("brightest") }) {
        combineCombo_->addItem(AmbiLight::combineModes().value(key, key), key);
    }
    modeRow->addWidget(modeLabel);
    modeRow->addWidget(combineCombo_, 1);
    layout->addLayout(modeRow);
    // --- Итоговый цвет: среднее по областям и то, что получит лента ---
    auto* colorRow = new QHBoxLayout;
    colorRow->setSpacing(8);
    auto* colorTitle = new QLabel(tr("Resulting color"), body);
    rawSwatch_ = new QLabel(body);
    rawSwatch_->setFixedSize(30, 20);
    rawSwatch_->setFrameShape(QFrame::Box);
    rawValueLabel_ = new QLabel(QStringLiteral("-"), body);
    rawValueLabel_->setProperty("role", "hex");
    finalSwatch_ = new QLabel(body);
    finalSwatch_->setFixedSize(30, 20);
    finalSwatch_->setFrameShape(QFrame::Box);
    finalValueLabel_ = new QLabel(QStringLiteral("-"), body);
    finalValueLabel_->setProperty("role", "hex");
    colorRow->addWidget(colorTitle);
    colorRow->addSpacing(8);
    colorRow->addWidget(rawSwatch_);
    colorRow->addWidget(rawValueLabel_);
    colorRow->addSpacing(8);
    colorRow->addWidget(finalSwatch_);
    colorRow->addWidget(finalValueLabel_);
    colorRow->addStretch();
    layout->addLayout(colorRow);
    // --- Монитор и способ захвата ---
    layout->addWidget(sectionTitle(tr("Monitor"), body));
    auto* screenRow = new QHBoxLayout;
    screenRow->setSpacing(6);
    screenCombo_ = new QComboBox(body);
    auto* refresh = new QPushButton(tr("Refresh"), body);
    refresh->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    screenRow->addWidget(screenCombo_, 1);
    screenRow->addWidget(refresh);
    layout->addLayout(screenRow);
    layout->addWidget(sectionTitle(tr("Capture method"), body));
    captureCombo_ = new QComboBox(body);
    for (const QString& key : { QStringLiteral("auto"), QStringLiteral("dxgi"),
                                QStringLiteral("gdi"), QStringLiteral("wgc") }) {
        captureCombo_->addItem(AmbiLight::captureModeName(AmbiLight::captureModeFromString(key)), key);
    }
    layout->addWidget(captureCombo_);
    layout->addStretch();
    // --- Кнопки внизу панели, как в макете ---
    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    auto* reset = new QPushButton(tr("Reset"), body);
    reset->setToolTip(tr("Return the capture area settings to the defaults"));
    auto* cancel = new QPushButton(tr("Cancel"), body);
    auto* apply = new QPushButton(tr("Apply"), body);
    auto* ok = new QPushButton(tr("OK"), body);
    ok->setProperty("accent", true);
    actions->addWidget(reset);
    actions->addStretch();
    actions->addWidget(cancel);
    actions->addWidget(apply);
    actions->addWidget(ok);
    layout->addLayout(actions);
    connect(bandSlider_, &QSlider::valueChanged, this, [this](int value) {
        bandPct_ = value;
        bandValue_->setText(QString::number(value) + QStringLiteral("%"));
        if (!updating_) updateRegion();
    });
    connect(frequencySlider_, &QSlider::valueChanged, this, [this](int value) {
        frequency_ = value;
        frequencyValue_->setText(tr("%1 Hz").arg(value));
        if (ambilight_) ambilight_->setFrequency(frequency_);
    });
    connect(boostSlider_, &QSlider::valueChanged, this, [this](int value) {
        boost_ = value / 100.0;
        boostValue_->setText(QString::number(boost_, 'f', 2) + QStringLiteral("x"));
        if (!updating_) updateRegion();
    });
    connect(minLevelSlider_, &QSlider::valueChanged, this, [this](int value) {
        minLevel_ = value;
        minLevelValue_->setText(QString::number(value));
        if (!updating_) updateRegion();
    });
    connect(smoothSlider_, &QSlider::valueChanged, this, [this](int value) {
        smooth_ = value / 100.0;
        smoothValue_->setText(QString::number(value) + QStringLiteral("%"));
        if (ambilight_) ambilight_->setSmooth(smooth_);
    });
    connect(autoBrightCheck_, &QCheckBox::toggled, this, [this](bool on) {
        autoBright_ = on;
        if (!updating_) updateRegion();
    });
    connect(combineCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        combine_ = combineCombo_->currentData().toString();
        if (!updating_) updateRegion();
    });
    connect(refresh, &QPushButton::clicked, this, &AmbiEditor::refreshScreens);
    connect(captureCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        captureKey_ = captureCombo_->currentData().toString();
    });
    connect(screenCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        screenIndex_ = screenCombo_->currentData().toInt();
    });
    connect(reset, &QPushButton::clicked, this, &AmbiEditor::resetAll);
    connect(cancel, &QPushButton::clicked, this, &QWidget::close);
    connect(apply, &QPushButton::clicked, this, &AmbiEditor::apply);
    connect(ok, &QPushButton::clicked, this, [this] { AmbiEditor::apply(); close(); });
    return page;
}
QWidget* AmbiEditor::buildLayersPanel(QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(sectionTitle(tr("Layers"), box));
    layerList_ = new QListWidget(box);
    layerList_->setSelectionMode(QAbstractItemView::SingleSelection);
    layerList_->setMinimumHeight(150);
    layerList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layerList_->setTextElideMode(Qt::ElideRight);
    layout->addWidget(layerList_, 1);
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(4);
    addLayerButton_ = bareIconButton(box, icons::Id::Plus, tr("Add a layer"), false);
    duplicateLayerButton_ = bareIconButton(box, icons::Id::Duplicate, tr("Duplicate the layer"), false);
    deleteLayerButton_ = bareIconButton(box, icons::Id::Trash, tr("Delete the layer (Del)"), false);
    undoButton_ = bareIconButton(box, icons::Id::Undo, tr("Undo (Ctrl+Z)"), false);
    redoButton_ = bareIconButton(box, icons::Id::Redo, tr("Redo (Ctrl+Shift+Z)"), false);
    for (QToolButton* button : { addLayerButton_, duplicateLayerButton_, deleteLayerButton_,
                                 undoButton_, redoButton_ }) {
        buttons->addWidget(button);
    }
    layout->addLayout(buttons);
    connect(layerList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row == activeLayer_) return;
        activeLayer_ = row;
        if (canvas_) canvas_->setActiveZone(activeLayer_);
        updateLayerInspector();
    });
    connect(addLayerButton_, &QToolButton::clicked, this, &AmbiEditor::addLayer);
    connect(duplicateLayerButton_, &QToolButton::clicked, this, &AmbiEditor::duplicateLayer);
    connect(deleteLayerButton_, &QToolButton::clicked, this, &AmbiEditor::deleteLayer);
    connect(undoButton_, &QToolButton::clicked, this, [this] { stepHistory(-1); });
    connect(redoButton_, &QToolButton::clicked, this, [this] { stepHistory(1); });
    return box;
}
QWidget* AmbiEditor::buildLayerPropertiesPanel(QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(sectionTitle(tr("Layer properties"), box));
    auto* nameRow = new QHBoxLayout;
    nameRow->setSpacing(6);
    layerName_ = new QLineEdit(box);
    layerName_->setPlaceholderText(tr("Layer name"));
    layerEnabled_ = new QCheckBox(tr("On"), box);
    layerEnabled_->setToolTip(tr("Include this layer in the capture"));
    nameRow->addWidget(layerName_, 1);
    nameRow->addWidget(layerEnabled_);
    layout->addLayout(nameRow);
    const auto sliderRow = [box, this, layout](const QString& caption, const QString& tip, int min,
                                                int max, int initial, QSlider*& slider,
                                                QLabel*& value) {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        auto* label = new QLabel(caption, box);
        label->setToolTip(tip);
        slider = new QSlider(Qt::Horizontal, box);
        slider->setRange(min, max);
        slider->setValue(initial);
        slider->setToolTip(tip);
        value = valueLabel(QString::number(initial), box);
        value->setMinimumWidth(48);
        row->addWidget(label);
        row->addWidget(slider, 1);
        row->addWidget(value);
        layout->addLayout(row);
    };
    // Отдельный «Colour boost» у слоя убран: он считал почти то же, что и
    // насыщенность, и путался с ней. Слой берёт общий boost из простой вкладки.
    sliderRow(tr("Share:"), tr("How much this layer affects the final colour"),
              0, 300, 100, layerWeight_, layerWeightValue_);
    sliderRow(tr("Brightness:"), tr("Brightness of this layer only"),
              0, 200, 100, layerBrightness_, layerBrightnessValue_);
    sliderRow(tr("Saturation:"), tr("Colourfulness of this layer only"),
              0, 200, 100, layerSaturation_, layerSaturationValue_);
    auto* boostRow = new QHBoxLayout;
    boostRow->setSpacing(6);
    auto* boostLabel = new QLabel(tr("Smoothing:"), box);
    boostLabel->setToolTip(tr("Blends the new colour with the previous frame so the strip does not flicker"));
    layerSmooth_ = new QSlider(Qt::Horizontal, box);
    // Подсказку задаём только после создания ползунка: обращение к нему раньше
    // работает по мусорному указателю и роняет редактор.
    layerSmooth_->setToolTip(boostLabel->toolTip());
    layerSmooth_->setRange(0, 90);
    layerSmooth_->setValue(30);
    layerSmoothValue_ = valueLabel(QStringLiteral("30%"), box);
    layerSmoothGlobal_ = new QCheckBox(tr("global"), box);
    layerSmoothGlobal_->setToolTip(tr("Use the smoothing from the Simple tab"));
    boostRow->addWidget(boostLabel);
    boostRow->addWidget(layerSmooth_, 1);
    boostRow->addWidget(layerSmoothValue_);
    boostRow->addWidget(layerSmoothGlobal_);
    layout->addLayout(boostRow);
    auto* boostAutoRow = new QHBoxLayout;
// --- Кривые слоя, как в Photoshop ---
    auto* curveHeader = new QHBoxLayout;
    curveHeader->setSpacing(6);
    curveHeader->addWidget(new QLabel(tr("Curves:"), box));
    curveChannel_ = new QComboBox(box);
    curveChannel_->addItem(tr("RGB"), 0);
    curveChannel_->addItem(tr("Red"), 1);
    curveChannel_->addItem(tr("Green"), 2);
    curveChannel_->addItem(tr("Blue"), 3);
    curveChannel_->setToolTip(tr("Which channel the curve changes"));
    auto* curveReset = new QToolButton(box);
    curveReset->setText(tr("Reset"));
    curveReset->setToolTip(tr("Return the curve to a straight line"));
    curveHeader->addWidget(curveChannel_, 1);
    curveHeader->addWidget(curveReset);
    layout->addLayout(curveHeader);
    curveEditor_ = new CurveEditor(box);
    curveEditor_->setToolTip(tr("Drag the points; click to add, right-click to remove"));
    layout->addWidget(curveEditor_);
    connect(curveChannel_, &QComboBox::currentIndexChanged, this, [this](int) {
        loadCurveForActiveLayer();
});
    connect(curveReset, &QToolButton::clicked, this, [this] {
        if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
        if (curveEditor_) curveEditor_->resetToLinear();
});
    connect(curveEditor_, &CurveEditor::curvesChanged, this, [this] {
        if (activeLayer_ < 0 || activeLayer_ >= zones_.size() || !curveEditor_) return;
        CaptureZone& zone = zones_[activeLayer_];
        const QVector<QPointF> points = curveEditor_->points();
        switch (curveChannel_ ? curveChannel_->currentData().toInt() : 0) {
        case 1: zone.curveR = points; break;
        case 2: zone.curveG = points; break;
        case 3: zone.curveB = points; break;
        default: zone.curveRgb = points; break;
        }
        applyLayerProperty();
    });
    // Отдельной строки «Use global» больше нет: галка «global» стоит рядом с
    // ползунком сглаживания, а boost слоя убран вовсе.
    connect(layerName_, &QLineEdit::editingFinished, this, [this] {
        if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
        zones_[activeLayer_].name = layerName_->text().isEmpty()
                                        ? tr("Layer %1").arg(activeLayer_ + 1)
                                        : layerName_->text();
        refreshLayerList();
    });
    connect(layerEnabled_, &QCheckBox::toggled, this, [this](bool on) {
        if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
        zones_[activeLayer_].enabled = on;
        applyLayerProperty();
    });
    connect(layerWeight_, &QSlider::valueChanged, this, [this](int value) {
        layerWeightValue_->setText(QString::number(value / 100.0, 'f', 2));
        if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
        zones_[activeLayer_].weight = value / 100.0;
        applyLayerProperty();
    });
    // Собственного boost у слоя больше нет: значение всегда -1.0, то есть
    // слой берёт общий boost из простой вкладки.
    connect(layerSmooth_, &QSlider::valueChanged, this, [this](int value) {
        layerSmoothValue_->setText(QString::number(value) + QStringLiteral("%"));
        if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
        zones_[activeLayer_].smooth = layerSmoothGlobal_->isChecked() ? -1.0 : value / 100.0;
        applyLayerProperty();
    });
    connect(layerSmoothGlobal_, &QCheckBox::toggled, this, [this](bool on) {
        if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
        zones_[activeLayer_].smooth = on ? -1.0 : layerSmooth_->value() / 100.0;
        applyLayerProperty();
    });
    connect(layerBrightness_, &QSlider::valueChanged, this, [this](int value) {
        layerBrightnessValue_->setText(QString::number(value / 100.0, 'f', 2));
        if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
        zones_[activeLayer_].brightness = value / 100.0;
        applyLayerProperty();
    });
    connect(layerSaturation_, &QSlider::valueChanged, this, [this](int value) {
        layerSaturationValue_->setText(QString::number(value / 100.0, 'f', 2));
        if (activeLayer_ < 0 || activeLayer_ >= zones_.size()) return;
        zones_[activeLayer_].saturation = value / 100.0;
        applyLayerProperty();
    });
    return box;
}
// История свёрнута по умолчанию: она нужна для отката, но не должна занимать
// место, пока пользователь ею не пользуется.
QWidget* AmbiEditor::buildHistoryPanel(QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto* header = new QToolButton(box);
    header->setCheckable(true);
    header->setAutoRaise(true);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setIcon(icons::make(icons::Id::ArrowDown));
    header->setText(tr("History"));
    header->setToolTip(tr("List of the last changes"));
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(header);
    historyList_ = new QListWidget(box);
    historyList_->setVisible(false);
    historyList_->setMaximumHeight(140);
    historyList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    historyList_->setToolTip(tr("Click a step to go back to it"));
    layout->addWidget(historyList_);
    connect(header, &QToolButton::toggled, this, [this, header](bool on) {
        header->setIcon(icons::make(on ? icons::Id::ArrowUp : icons::Id::ArrowDown));
        if (historyList_) historyList_->setVisible(on);
    });
    connect(historyList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= history_.size()) return;
        stepHistory(row - historyIndex_);
    });
    return box;
}
QWidget* AmbiEditor::buildStatusBar(QWidget* parent)
{
    auto* bar = new QWidget(parent);
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(12);
    cursorLabel_ = new QLabel(bar);
    cursorLabel_->setProperty("role", "status");
    zoomLabel_ = new QLabel(bar);
    zoomLabel_->setProperty("role", "status");
    row->addWidget(cursorLabel_);
    row->addWidget(zoomLabel_);
    row->addStretch();
    return bar;
}

} // namespace elkbledom

