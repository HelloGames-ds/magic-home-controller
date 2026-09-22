#include "TrayPopup.h"

#include "ColorWheel.h"
#include "EffectEngine.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace elkbledom {
namespace {

QString squareButtonStyle()
{
    return QStringLiteral(
        "QPushButton { font-size: 18px; padding: 0; background-color: #2b2b35;"
        " border: 1px solid #3a3a48; border-radius: 8px; color: #e0e0e6; }"
        "QPushButton:hover { background-color: #7b2cbf; border-color: #9d4edd; color: white; }");
}

} // namespace

TrayPopup::TrayPopup(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(300);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("popupCard"));
    outer->addWidget(card);

    auto* content = new QVBoxLayout(card);
    content->setContentsMargins(14, 14, 14, 14);
    content->setSpacing(10);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(tr("Backlight"), card);
    title->setProperty("role", QStringLiteral("title"));
    header->addWidget(title);
    header->addStretch(1);
    previewDot_ = new QLabel(card);
    previewDot_->setFixedSize(20, 20);
    header->addWidget(previewDot_);
    content->addLayout(header);

    wheel_ = new ColorWheel(card);
    wheel_->setFixedHeight(180);
    content->addWidget(wheel_);
    connect(wheel_, &ColorWheel::colorChanged, this,
            [this](int red, int green, int blue) {
                updatePreview(red, green, blue);
                Q_EMIT colorChanged(red, green, blue);
            });

    auto* brightnessRow = new QHBoxLayout;
    brightnessRow->addWidget(new QLabel(tr("Brightness"), card));
    brightness_ = new QSlider(Qt::Horizontal, card);
    brightness_->setRange(1, 100);
    brightness_->setValue(100);
    brightnessRow->addWidget(brightness_, 1);
    brightnessValue_ = new QLabel(QStringLiteral("100%"), card);
    brightnessValue_->setProperty("role", QStringLiteral("value"));
    brightnessRow->addWidget(brightnessValue_);
    content->addLayout(brightnessRow);
    connect(brightness_, &QSlider::valueChanged, this, [this](int value) {
        brightnessValue_->setText(QStringLiteral("%1%").arg(value));
        Q_EMIT brightnessChanged(value);
    });

    auto* tauRow = new QHBoxLayout;
    tauRow->addWidget(new QLabel(tr("Smooth"), card));
    tau_ = new QSlider(Qt::Horizontal, card);
    tau_->setRange(20, 500);
    tau_->setValue(200);
    tauRow->addWidget(tau_, 1);
    tauValue_ = new QLabel(tr("200 ms"), card);
    tauValue_->setProperty("role", QStringLiteral("value"));
    tauRow->addWidget(tauValue_);
    content->addLayout(tauRow);
    connect(tau_, &QSlider::valueChanged, this, [this](int value) {
        tauValue_->setText(tr("%1 ms").arg(value));
        Q_EMIT smoothingTauChanged(value);
    });

    auto* sourceRow = new QHBoxLayout;
    modeCombo_ = new QComboBox(card);
    modeCombo_->addItem(tr("Effect"), QStringLiteral("effect"));
    modeCombo_->addItem(QStringLiteral("Ambilight"), QStringLiteral("ambilight"));
    sourceRow->addWidget(modeCombo_, 1);
    effectCombo_ = new QComboBox(card);
    const QStringList effectIds = EffectEngine::effects();
    const auto& effectLabels = EffectEngine::labels();
    for (const QString& id : effectIds)
        effectCombo_->addItem(effectLabels.value(id), id);
    sourceRow->addWidget(effectCombo_, 1);
    content->addLayout(sourceRow);

    connect(modeCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        updateModeControls();
        Q_EMIT modeChanged(mode());
    });
    connect(effectCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (mode() != QStringLiteral("effect")) {
            const QSignalBlocker blocker(modeCombo_);
            modeCombo_->setCurrentIndex(modeCombo_->findData(QStringLiteral("effect")));
            updateModeControls();
            Q_EMIT modeChanged(QStringLiteral("effect"));
        } else {
            updateModeControls();
        }
        Q_EMIT effectChanged(effect());
    });

    auto* buttons = new QHBoxLayout;
    auto* onButton = new QPushButton(tr("On"), card);
    onButton->setProperty("accent", true);
    onButton->setMinimumHeight(36);
    auto* offButton = new QPushButton(tr("Off"), card);
    offButton->setProperty("danger", true);
    offButton->setMinimumHeight(36);
    auto* settingsButton = new QPushButton(QStringLiteral("⚙"), card);
    settingsButton->setFixedSize(38, 38);
    settingsButton->setToolTip(tr("Settings"));
    settingsButton->setStyleSheet(squareButtonStyle());
    auto* windowButton = new QPushButton(QStringLiteral("⛶"), card);
    windowButton->setFixedSize(38, 38);
    windowButton->setToolTip(tr("Open full window"));
    windowButton->setStyleSheet(squareButtonStyle());
    buttons->addWidget(onButton);
    buttons->addWidget(offButton);
    buttons->addWidget(settingsButton);
    buttons->addWidget(windowButton);
    content->addLayout(buttons);

    connect(onButton, &QPushButton::clicked, this, [this] { Q_EMIT powerRequested(true); });
    connect(offButton, &QPushButton::clicked, this, [this] { Q_EMIT powerRequested(false); });
    connect(settingsButton, &QPushButton::clicked, this, &TrayPopup::settingsRequested);
    connect(windowButton, &QPushButton::clicked, this, &TrayPopup::windowRequested);

    updatePreview(123, 44, 191);
    updateModeControls();
}

void TrayPopup::syncState(int red, int green, int blue, int brightness,
                          const QString& newMode, const QString& newEffect,
                          int smoothingTauMs)
{
    {
        const QSignalBlocker blocker(wheel_);
        wheel_->setVisualRgb(red, green, blue);
    }
    {
        const QSignalBlocker blocker(brightness_);
        brightness_->setValue(brightness);
    }
    brightnessValue_->setText(QStringLiteral("%1%").arg(brightness_->value()));
    {
        const QSignalBlocker blocker(tau_);
        tau_->setValue(smoothingTauMs);
    }
    tauValue_->setText(tr("%1 ms").arg(tau_->value()));
    updatePreview(red, green, blue);

    {
        const QSignalBlocker blocker(modeCombo_);
        const int index = modeCombo_->findData(newMode);
        if (index >= 0)
            modeCombo_->setCurrentIndex(index);
    }
    {
        const QSignalBlocker blocker(effectCombo_);
        const int index = effectCombo_->findData(newEffect);
        if (index >= 0)
            effectCombo_->setCurrentIndex(index);
    }
    updateModeControls();
}

QString TrayPopup::mode() const
{
    return modeCombo_->currentData().toString();
}

QString TrayPopup::effect() const
{
    return effectCombo_->currentData().toString();
}

void TrayPopup::updateModeControls()
{
    const bool effectMode = mode() == QStringLiteral("effect");
    effectCombo_->setEnabled(effectMode);
    wheel_->setEnabled(effectMode && effect() == QStringLiteral("static"));
}

void TrayPopup::updatePreview(int red, int green, int blue)
{
    previewDot_->setStyleSheet(QStringLiteral(
        "background: rgb(%1,%2,%3); border-radius:10px;").arg(red).arg(green).arg(blue));
}

} // namespace elkbledom
