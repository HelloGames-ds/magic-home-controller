#include "AmbiEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace elkbledom {

CanvasPreview::CanvasPreview(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(400, 260);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
void CanvasPreview::setImage(const QPixmap& value) { image_ = value; update(); }
void CanvasPreview::clearImage() { image_ = QPixmap(); update(); }
void CanvasPreview::setRegion(const QString& value) { region_ = value; editMode_ = value == "custom"; update(); }
void CanvasPreview::setBandPct(int value) { bandPct_ = std::clamp(value, 1, 80); update(); }
void CanvasPreview::setCustomRect(const QRectF& value) { customRect_ = value; update(); }
void CanvasPreview::setPreviewData(const QVector<QRect>& rects, const QVector<QColor>& samples,
                                   const QColor& combined)
{
    rects_ = rects; sampleColors_ = samples; combined_ = combined; update();
}
QSize CanvasPreview::imageSize() const { return image_.isNull() ? QSize(1920, 1080) : image_.size(); }

void CanvasPreview::mousePressEvent(QMouseEvent* event)
{
    if (editMode_ && event->button() == Qt::LeftButton && imageRect_.contains(event->position().toPoint())) {
        dragStart_ = event->position().toPoint(); dragging_ = true;
    }
}
void CanvasPreview::mouseMoveEvent(QMouseEvent* event)
{
    if (editMode_ && dragging_) updateCustom(dragStart_, event->position().toPoint());
}
void CanvasPreview::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && dragging_) {
        updateCustom(dragStart_, event->position().toPoint()); dragging_ = false;
    }
}
void CanvasPreview::updateCustom(const QPoint& first, const QPoint& second)
{
    if (imageRect_.isEmpty()) return;
    const QPoint a(std::clamp(std::min(first.x(), second.x()), imageRect_.left(), imageRect_.right()),
                   std::clamp(std::min(first.y(), second.y()), imageRect_.top(), imageRect_.bottom()));
    const QPoint b(std::clamp(std::max(first.x(), second.x()), imageRect_.left(), imageRect_.right()),
                   std::clamp(std::max(first.y(), second.y()), imageRect_.top(), imageRect_.bottom()));
    QRectF normalized((a.x() - imageRect_.left()) / double(imageRect_.width()),
                      (a.y() - imageRect_.top()) / double(imageRect_.height()),
                      (b.x() - a.x()) / double(imageRect_.width()),
                      (b.y() - a.y()) / double(imageRect_.height()));
    if (normalized.width() < .02 || normalized.height() < .02) return;
    customRect_ = normalized;
    Q_EMIT customRectChanged(normalized.x(), normalized.y(), normalized.width(), normalized.height());
    update();
}

void CanvasPreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), QColor("#14141a"));
    const QSize source = imageSize();
    QSize target = source;
    target.scale(std::max(1, width() - 32), std::max(1, height() - 32), Qt::KeepAspectRatio);
    imageRect_ = QRect(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2), target);
    if (!image_.isNull()) painter.drawPixmap(imageRect_, image_);
    else {
        painter.setBrush(QColor("#23232b")); painter.setPen(QPen(QColor("#3a3a48"), 2));
        painter.drawRoundedRect(imageRect_, 10, 10);
    }
    const double sx = imageRect_.width() / double(source.width());
    const double sy = imageRect_.height() / double(source.height());
    for (int i = 0; i < rects_.size(); ++i) {
        const QRect r = rects_.at(i);
        QRect shown(imageRect_.x() + int(r.x() * sx), imageRect_.y() + int(r.y() * sy),
                    std::max(2, int(r.width() * sx)), std::max(2, int(r.height() * sy)));
        QColor color = i < sampleColors_.size() ? sampleColors_.at(i) : QColor(255, 255, 255);
        color.setAlpha(90); painter.setBrush(color);
        painter.setPen(QPen(QColor("#b070ff"), 1, Qt::DashLine)); painter.drawRect(shown);
    }
    if (region_ == "custom") {
        QRect custom(imageRect_.x() + int(customRect_.x() * imageRect_.width()),
                     imageRect_.y() + int(customRect_.y() * imageRect_.height()),
                     int(customRect_.width() * imageRect_.width()), int(customRect_.height() * imageRect_.height()));
        painter.setBrush(Qt::NoBrush); painter.setPen(QPen(QColor("#b070ff"), 2)); painter.drawRect(custom);
    }
    painter.setBrush(combined_); painter.setPen(QPen(QColor("#b070ff"), 2));
    painter.drawRoundedRect(width() - 52, 12, 40, 40, 6, 6);
    if (editMode_) {
        painter.setPen(QColor("#a0a0b0"));
        painter.drawText(QRect(imageRect_.x(), imageRect_.bottom() - 20, imageRect_.width(), 18),
                         Qt::AlignCenter, tr("drag the mouse to define a custom area"));
    }
}

AmbiEditor::AmbiEditor(AmbiLight* ambilight, QWidget* parent)
    : QDialog(parent), ambilight_(ambilight)
{
    Q_ASSERT(ambilight_);
    region_ = ambilight_->region(); bandPct_ = ambilight_->bandPct(); boost_ = ambilight_->boost();
    smooth_ = ambilight_->smooth(); minLevel_ = ambilight_->minLevel(); autoBright_ = ambilight_->autoBright();
    frequency_ = ambilight_->frequency(); screenIndex_ = ambilight_->screenIndex();
    combine_ = ambilight_->combineMode(); customRect_ = ambilight_->customRect();
    captureKey_ = AmbiLight::captureModeKey(ambilight_->captureMode());
    setWindowTitle(tr("Ambilight editor")); setMinimumSize(980, 700);
    buildUi(); syncControls();
    QTimer::singleShot(0, this, &AmbiEditor::grabScreenSnapshot);
}

static QHBoxLayout* sliderRow(QSlider*& slider, QLabel*& label, int low, int high)
{
    slider = new QSlider(Qt::Horizontal); slider->setRange(low, high); label = new QLabel;
    auto* row = new QHBoxLayout; row->addWidget(slider, 1); row->addWidget(label); return row;
}

void AmbiEditor::buildUi()
{
    auto* root = new QHBoxLayout(this); auto* left = new QVBoxLayout; canvas_ = new CanvasPreview;
    left->addWidget(canvas_, 1); auto* imageButtons = new QHBoxLayout;
    auto* grab = new QPushButton(tr("Capture screen")); auto* load = new QPushButton(tr("Load image"));
    auto* clear = new QPushButton(tr("Remove image")); imageButtons->addWidget(grab); imageButtons->addWidget(load);
    imageButtons->addWidget(clear); imageButtons->addStretch(); left->addLayout(imageButtons);
    auto* info = new QHBoxLayout; sampledLabel_ = new QLabel(tr("Raw: —")); processedLabel_ = new QLabel(tr("After processing: —"));
    info->addWidget(sampledLabel_); info->addStretch(); info->addWidget(processedLabel_); left->addLayout(info);
    root->addLayout(left, 3);

    auto* right = new QVBoxLayout; auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    auto* panel = new QWidget; auto* controls = new QVBoxLayout(panel); scroll->setWidget(panel);
    auto* monitor = new QGroupBox(tr("Monitor")); auto* mf = new QFormLayout(monitor); screenCombo_ = new QComboBox;
    auto* refresh = new QPushButton(tr("Refresh list")); mf->addRow(tr("Screen:"), screenCombo_); mf->addRow({}, refresh); controls->addWidget(monitor);
    auto* captureGroup = new QGroupBox(tr("Screen capture mode")); auto* capf = new QFormLayout(captureGroup); captureCombo_ = new QComboBox;
    for (const QString& key : {QStringLiteral("auto"), QStringLiteral("dxgi"), QStringLiteral("gdi"), QStringLiteral("wgc")})
        captureCombo_->addItem(AmbiLight::captureModeName(AmbiLight::captureModeFromString(key)), key);
    capf->addRow(tr("Mode:"), captureCombo_); controls->addWidget(captureGroup);
    auto* area = new QGroupBox(tr("Capture area")); auto* af = new QFormLayout(area); regionCombo_ = new QComboBox;
    const QStringList regionOrder{"top","bottom","center","left","right","full","grid_3x3","grid_5x3","corner_tl","corner_tr","corner_bl","corner_br","custom"};
    for (const QString& key : regionOrder) regionCombo_->addItem(AmbiLight::regions().value(key), key);
    af->addRow(tr("Area:"), regionCombo_); af->addRow(tr("Band depth:"), sliderRow(bandSlider_, bandValue_, 1, 80)); controls->addWidget(area);
    auto* grouping = new QGroupBox(tr("Sample combining")); auto* gf = new QFormLayout(grouping); combineCombo_ = new QComboBox;
    for (const QString& key : {QString("average"), QString("brightest"), QString("saturated")}) combineCombo_->addItem(AmbiLight::combineModes().value(key), key);
    gf->addRow(tr("Mode:"), combineCombo_); controls->addWidget(grouping);
    auto* color = new QGroupBox(tr("Color processing")); auto* cf = new QFormLayout(color);
    cf->addRow(tr("Saturation:"), sliderRow(boostSlider_, boostValue_, 100, 300));
    cf->addRow(tr("Threshold (dark):"), sliderRow(minSlider_, minValue_, 0, 120)); autoBrightCheck_ = new QCheckBox(tr("Auto brightness (normalization)")); cf->addRow({}, autoBrightCheck_); controls->addWidget(color);
    auto* performance = new QGroupBox(tr("Performance")); auto* pf = new QFormLayout(performance);
    pf->addRow(tr("Smoothing:"), sliderRow(smoothSlider_, smoothValue_, 0, 90));
    pf->addRow(tr("Frequency:"), sliderRow(frequencySlider_, frequencyValue_, 1, 30)); controls->addWidget(performance); controls->addStretch(); right->addWidget(scroll, 1);
    auto* actions = new QHBoxLayout; auto* reset = new QPushButton(tr("Reset")); auto* cancel = new QPushButton(tr("Cancel")); auto* apply = new QPushButton(tr("Apply"));
    actions->addStretch(); actions->addWidget(reset); actions->addWidget(cancel); actions->addWidget(apply); right->addLayout(actions); root->addLayout(right, 2);

    connect(grab, &QPushButton::clicked, this, &AmbiEditor::grabScreenSnapshot); connect(load, &QPushButton::clicked, this, &AmbiEditor::loadImage);
    connect(clear, &QPushButton::clicked, this, &AmbiEditor::clearImage); connect(refresh, &QPushButton::clicked, this, &AmbiEditor::refreshScreens);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject); connect(reset, &QPushButton::clicked, this, &AmbiEditor::resetDefaults); connect(apply, &QPushButton::clicked, this, &AmbiEditor::applySettings);
    connect(canvas_, &CanvasPreview::customRectChanged, this, [this](double x,double y,double w,double h){ customRect_={x,y,w,h}; recompute(); });
    connect(screenCombo_, &QComboBox::currentIndexChanged, this, [this](int){ screenIndex_=screenCombo_->currentData().toInt(); grabScreenSnapshot(); });
    connect(captureCombo_, &QComboBox::currentIndexChanged, this, [this](int){ captureKey_=captureCombo_->currentData().toString(); });
    connect(regionCombo_, &QComboBox::currentIndexChanged, this, [this](int){ region_=regionCombo_->currentData().toString(); canvas_->setRegion(region_); recompute(); });
    connect(combineCombo_, &QComboBox::currentIndexChanged, this, [this](int){ combine_=combineCombo_->currentData().toString(); recompute(); });
    connect(bandSlider_, &QSlider::valueChanged, this, [this](int v){ bandPct_=v; bandValue_->setText(QString::number(v)+"%"); canvas_->setBandPct(v); recompute(); });
    connect(boostSlider_, &QSlider::valueChanged, this, [this](int v){ boost_=v/100.; boostValue_->setText(QString::number(boost_,'f',1)+"x"); recompute(); });
    connect(minSlider_, &QSlider::valueChanged, this, [this](int v){ minLevel_=v; minValue_->setText(QString::number(v)); recompute(); });
    connect(autoBrightCheck_, &QCheckBox::toggled, this, [this](bool v){ autoBright_=v; recompute(); });
    connect(smoothSlider_, &QSlider::valueChanged, this, [this](int v){ smooth_=v/100.; smoothValue_->setText(QString::number(v)+"%"); });
    connect(frequencySlider_, &QSlider::valueChanged, this, [this](int v){ frequency_=v; frequencyValue_->setText(tr("%1 Hz").arg(v)); });
}

void AmbiEditor::syncControls()
{
    refreshScreens();
    { QSignalBlocker b(regionCombo_); regionCombo_->setCurrentIndex(regionCombo_->findData(region_)); }
    { QSignalBlocker b(combineCombo_); combineCombo_->setCurrentIndex(combineCombo_->findData(combine_)); }
    { QSignalBlocker b(captureCombo_); captureCombo_->setCurrentIndex(captureCombo_->findData(captureKey_)); }
    const QList<QPair<QSlider*,int>> values{{bandSlider_,bandPct_},{boostSlider_,int(boost_*100)},{minSlider_,minLevel_},{smoothSlider_,int(smooth_*100)},{frequencySlider_,int(frequency_)}};
    for (auto value : values) { QSignalBlocker b(value.first); value.first->setValue(value.second); }
    { QSignalBlocker b(autoBrightCheck_); autoBrightCheck_->setChecked(autoBright_); }
    bandValue_->setText(QString::number(bandPct_)+"%"); boostValue_->setText(QString::number(boost_,'f',1)+"x"); minValue_->setText(QString::number(minLevel_));
    smoothValue_->setText(QString::number(int(smooth_*100))+"%"); frequencyValue_->setText(tr("%1 Hz").arg(int(frequency_)));
    canvas_->setRegion(region_); canvas_->setBandPct(bandPct_); canvas_->setCustomRect(customRect_);
}

void AmbiEditor::refreshScreens()
{
    QSignalBlocker blocker(screenCombo_); screenCombo_->clear();
    for (const auto& screen : AmbiLight::listScreens()) screenCombo_->addItem(QString("%1  ·  %2×%3%4").arg(screen.name).arg(screen.size.width()).arg(screen.size.height()).arg(screen.primary ? tr(" (primary)") : ""), screen.index);
    int row = screenCombo_->findData(screenIndex_); if (row < 0 && screenCombo_->count()) { row=0; screenIndex_=screenCombo_->itemData(0).toInt(); }
    screenCombo_->setCurrentIndex(row);
}
void AmbiEditor::grabScreenSnapshot()
{
    const auto screens = QGuiApplication::screens(); if (screens.isEmpty()) return;
    screenIndex_ = std::clamp(screenIndex_, 0, static_cast<int>(screens.size())-1); QPixmap shot=screens.at(screenIndex_)->grabWindow(0); if (shot.isNull()) return;
    if (shot.width()>1600) shot=shot.scaledToWidth(1600,Qt::SmoothTransformation); pixmap_=shot; canvas_->setImage(shot); recompute();
}
void AmbiEditor::loadImage()
{
    const QString path=QFileDialog::getOpenFileName(this,tr("Choose image"),{},tr("Images (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*.*)")); if(path.isEmpty()) return;
    QPixmap image(path); if(image.isNull()){ QMessageBox::warning(this,tr("Error"),tr("Failed to load the image.")); return; }
    if(image.width()>1600) image=image.scaledToWidth(1600,Qt::SmoothTransformation); pixmap_=image; canvas_->setImage(image); recompute();
}
void AmbiEditor::clearImage(){ pixmap_=QPixmap(); canvas_->clearImage(); canvas_->setPreviewData({}, {}, QColor(0,0,0)); sampledLabel_->setText(tr("Raw: —")); processedLabel_->setText(tr("After processing: —")); }
void AmbiEditor::recompute()
{
    if(pixmap_.isNull()) return; const QImage image=pixmap_.toImage(); const auto rects=ambilight_->buildRects(image.width(),image.height(),region_,bandPct_,customRect_); QVector<QColor> colors;
    for(QRect r:rects){ r=r.intersected(image.rect()); if(r.isEmpty()) continue; qint64 rr=0,g=0,b=0,n=0; const int cx=r.center().x(),cy=r.center().y();
        for(int y:{cy-r.height()/4,cy,cy+r.height()/4}) for(int x:{cx-r.width()/4,cx,cx+r.width()/4}) if(image.valid(x,y)){ QColor c=image.pixelColor(x,y); rr+=c.red();g+=c.green();b+=c.blue();++n; }
        if(n) colors.push_back(QColor(int(rr/n),int(g/n),int(b/n))); }
    if(colors.isEmpty()) return; QColor raw=ambilight_->combine(colors,combine_); QColor used=ambilight_->processRaw(raw,boost_,minLevel_,autoBright_?1:0);
    canvas_->setPreviewData(rects,colors,used); sampledLabel_->setText(tr("Raw: %1,%2,%3").arg(raw.red()).arg(raw.green()).arg(raw.blue())); processedLabel_->setText(tr("After processing: %1,%2,%3").arg(used.red()).arg(used.green()).arg(used.blue()));
}
void AmbiEditor::resetDefaults(){ region_="top";bandPct_=8;boost_=1.3;smooth_=.3;minLevel_=15;autoBright_=false;frequency_=3.;combine_="average";customRect_={0,0,1,.1};syncControls();recompute(); }
QVariantMap AmbiEditor::settings() const { return {{"ambi_region",region_},{"ambi_band",bandPct_},{"ambi_boost",boost_},{"ambi_smooth",smooth_},{"ambi_min",minLevel_},{"ambi_auto",autoBright_},{"ambi_freq",frequency_},{"ambi_screen",screenIndex_},{"ambi_combine",combine_},{"ambi_capture",captureKey_},{"ambi_rect",customRect_}}; }
void AmbiEditor::setSettings(const QVariantMap& s){ region_=s.value("ambi_region",region_).toString();bandPct_=s.value("ambi_band",bandPct_).toInt();boost_=s.value("ambi_boost",boost_).toDouble();smooth_=s.value("ambi_smooth",smooth_).toDouble();minLevel_=s.value("ambi_min",minLevel_).toInt();autoBright_=s.value("ambi_auto",autoBright_).toBool();frequency_=s.value("ambi_freq",frequency_).toDouble();screenIndex_=s.value("ambi_screen",screenIndex_).toInt();combine_=s.value("ambi_combine",combine_).toString();captureKey_=s.value("ambi_capture",captureKey_).toString();customRect_=s.value("ambi_rect",customRect_).toRectF();syncControls();recompute(); }
void AmbiEditor::applySettings(){ Q_EMIT settingsApplied(settings()); accept(); }

} // namespace elkbledom
