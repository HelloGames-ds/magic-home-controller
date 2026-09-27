#include "AmbiEditor.h"
#include "AmbiCanvas.h"
#include "AmbiLight.h"
#include "AppLog.h"
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
namespace {
QLabel* sectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", "section");
    return label;
}
QLabel* valueLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", "value");
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return label;
}
QToolButton* actionButton(QWidget* parent, icons::Id id, const QString& text, const QString& tip)
{
    auto* button = new QToolButton(parent);
    button->setIcon(icons::make(id));
    button->setIconSize(QSize(18, 18));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setText(text);
    if (!tip.isEmpty()) button->setToolTip(tip);
    button->setAutoRaise(false);
    button->setFocusPolicy(Qt::NoFocus);
    button->setProperty("action", true);
    return button;
}
QToolButton* bareIconButton(QWidget* parent, icons::Id id, const QString& tip, bool checkable)
{
    auto* button = new QToolButton(parent);
    button->setIcon(icons::make(id));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(tip);
    button->setCheckable(checkable);
    button->setAutoRaise(true);
    button->setFixedSize(30, 28);
    button->setFocusPolicy(Qt::NoFocus);
    button->setProperty("bare", true);
    return button;
}
// Значок пресета: схематичная рамка того, что захватывается. Рисуется вручную,
// чтобы не тащить картинки для каждой области.
QIcon regionIcon(const QString& key, const QColor& color)
{
    QPixmap pixmap(26, 18);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.4));
    painter.setBrush(Qt::NoBrush);
    const QRectF box(1.5, 1.5, 23, 15);
    const double band = 0.34;
    if (key == QStringLiteral("full")) {
        painter.drawRect(box);
    } else if (key == QStringLiteral("top")) {
        painter.drawRect(box);
        painter.drawRect(QRectF(box.left(), box.top(), box.width(), box.height() * band));
    } else if (key == QStringLiteral("bottom")) {
        painter.drawRect(box);
        painter.drawRect(QRectF(box.left(), box.bottom() - box.height() * band, box.width(),
                               box.height() * band));
    } else if (key == QStringLiteral("left")) {
        painter.drawRect(box);
        painter.drawRect(QRectF(box.left(), box.top(), box.width() * band, box.height()));
    } else if (key == QStringLiteral("right")) {
        painter.drawRect(box);
        painter.drawRect(QRectF(box.right() - box.width() * band, box.top(), box.width() * band,
                                box.height()));
    } else if (key == QStringLiteral("center")) {
        painter.drawRect(box);
        const double t = (1.0 - band) / 2.0;
        painter.drawRect(QRectF(box.left() + box.width() * t, box.top() + box.height() * t,
                                box.width() * (1.0 - 2 * t), box.height() * (1.0 - 2 * t)));
    } else if (key == QStringLiteral("grid_3x3") || key == QStringLiteral("grid_5x3")) {
        painter.drawRect(box);
        const int columns = key == QStringLiteral("grid_3x3") ? 3 : 5;
        for (int c = 1; c < columns; ++c)
            painter.drawLine(QPointF(box.left() + box.width() * c / columns, box.top()),
                             QPointF(box.left() + box.width() * c / columns, box.bottom()));
        for (int row = 1; row < 3; ++row)
            painter.drawLine(QPointF(box.left(), box.top() + box.height() * row / 3.0),
                             QPointF(box.right(), box.top() + box.height() * row / 3.0));
    } else if (key == QStringLiteral("corners")) {
        painter.drawRect(box);
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 90));
        const double cw = box.width() * 0.34, ch = box.height() * 0.42;
        painter.drawRect(QRectF(box.left(), box.top(), cw, ch));
        painter.drawRect(QRectF(box.right() - cw, box.top(), cw, ch));
        painter.drawRect(QRectF(box.left(), box.bottom() - ch, cw, ch));
        painter.drawRect(QRectF(box.right() - cw, box.bottom() - ch, cw, ch));
    } else {
        painter.drawRect(box);
    }
    return QIcon(pixmap);
}
// Строка настройки в одну линию: подпись слева, ползунок в середине, значение справа.
QHBoxLayout* settingRow(QWidget* parent, const QString& caption, const QString& tip,
                        QSlider*& slider, QLabel*& value, int min, int max, int initial)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    auto* label = new QLabel(caption, parent);
    label->setToolTip(tip);
    label->setMinimumWidth(140);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(min, max);
    slider->setValue(initial);
    slider->setToolTip(tip);
    value = valueLabel(QString::number(initial), parent);
    value->setMinimumWidth(58);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(label);
    row->addWidget(slider, 1);
    row->addWidget(value);
    return row;
}
// Значок и подсказка инструмента расширенного режима.
icons::Id toolIcons(AmbiCanvas::Tool tool)
{
    switch (tool) {
    case AmbiCanvas::Tool::Move: return icons::Id::Select;
    case AmbiCanvas::Tool::Rectangle: return icons::Id::Rectangle;
    case AmbiCanvas::Tool::Zoom: return icons::Id::ZoomIn;
    case AmbiCanvas::Tool::Hand: return icons::Id::Pan;
    }
    return icons::Id::Select;
}
QString toolToolTip(AmbiCanvas::Tool tool)
{
    switch (tool) {
    case AmbiCanvas::Tool::Move:
        return QCoreApplication::translate("AmbiEditor", "Move and resize layers (V)");
    case AmbiCanvas::Tool::Rectangle:
        return QCoreApplication::translate("AmbiEditor",
                                           "Rectangular selection (M). Shift keeps a square, "
                                           "Alt draws from the centre");
    case AmbiCanvas::Tool::Zoom:
        return QCoreApplication::translate("AmbiEditor", "Zoom in, Alt+click to zoom out (Z)");
    case AmbiCanvas::Tool::Hand:
        return QCoreApplication::translate("AmbiEditor", "Pan the image (H, or hold Space)");
    }
    return QString();
}
QSettings viewStore()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("Magic Home Controller"), QStringLiteral("editorView"));
}
} // namespace
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
