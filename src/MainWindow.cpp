#include "MainWindow.h"

#include "AmbiEditor.h"
#include "AmbiLight.h"
#include "AppLog.h"
#include "ColorSmoother.h"
#include "ColorWheel.h"
#include "DeviceScanner.h"
#include "EffectEngine.h"
#include "IconFactory.h"
#include "LogWindow.h"
#include "SessionShutdownFilter.h"
#include "TrayPopup.h"
#include "WifiWorker.h"
#include "config.h"
#include "protocol.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPalette>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace elkbledom {
namespace {
QFrame* card(QWidget* parent = nullptr) { auto* f = new QFrame(parent); f->setProperty("role", "card"); return f; }
QLabel* valueLabel(const QString& text, QWidget* parent = nullptr) { auto* l = new QLabel(text, parent); l->setProperty("role", "value"); return l; }
QScrollArea* scrollPage(QWidget*& body, QTabWidget* parent) { auto* s = new QScrollArea(parent); s->setWidgetResizable(true); s->setFrameShape(QFrame::NoFrame); body = new QWidget; s->setWidget(body); return s; }
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), store_(new SettingsStore(this)), wifi_(new WifiManager(this)),
      effects_(new EffectEngine(this)), ambilight_(new AmbiLight(this)),
      smoother_(new ColorSmoother(this)), scanner_(new DeviceScanner(this))
{
    settings_ = store_->load();
    currentColor_ = settings_.color;
    setWindowTitle(QString::fromLatin1(config::ApplicationDisplayName));
    setWindowIcon(IconFactory::appIcon());
    setMinimumSize(535, 865);

    auto* central = new QWidget(this); auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16,16,16,16); root->setSpacing(12);
    auto* header = new QHBoxLayout; auto* title = new QLabel(QString::fromLatin1(config::ApplicationName), central);
    title->setProperty("role", "title"); header->addWidget(title); header->addStretch();
    connectionDot_ = new QLabel(QStringLiteral("●"), central); connectionDot_->setProperty("role", "dot");
    header->addWidget(connectionDot_); root->addLayout(header);
    tabs_ = new QTabWidget(central); tabs_->addTab(createControlTab(), tr("Control"));
    tabs_->addTab(createEffectsTab(), tr("Effects")); tabs_->addTab(createAmbilightTab(), tr("Ambilight"));
    tabs_->addTab(createSettingsTab(), tr("Settings")); root->addWidget(tabs_, 1);
    statusLabel_ = new QLabel(tr("Ready"), central); statusLabel_->setProperty("role", "status"); root->addWidget(statusLabel_);
    setCentralWidget(central);

    popup_ = new TrayPopup(this); connectSignals(); applySettingsToUi(); setupTray();

    shutdownFilter_ = new SessionShutdownFilter(this);
    shutdownFilter_->setHandler([this] { sendShutdownPowerOff(); });
    qApp->installNativeEventFilter(shutdownFilter_);

    keepaliveTimer_ = new QTimer(this); keepaliveTimer_->setInterval(15000);
    connect(keepaliveTimer_, &QTimer::timeout, this, [this] {
        if (settings_.keepalive && connected_ && powerOn_ && settings_.mode == QStringLiteral("effect")
            && settings_.effect == QStringLiteral("static"))
            wifi_->send(protocol::colorCommand(currentColor_.red(), currentColor_.green(), currentColor_.blue()));
    }); keepaliveTimer_->start();
    iconTimer_ = new QTimer(this); iconTimer_->setInterval(250); iconTimer_->setSingleShot(true);
    connect(iconTimer_, &QTimer::timeout, this, [this] { refreshTrayIcon(); });
    smoothTestTimer_ = new QTimer(this); smoothTestTimer_->setInterval(1200);
    connect(smoothTestTimer_, &QTimer::timeout, this, &MainWindow::smoothTestStep);

    AppLog::setEnabled(settings_.loggingEnabled);
    wifi_->setAddress(settings_.address); wifi_->setMinimumInterval(settings_.netInterval);
    wifi_->setDeduplicationWindow(settings_.netDedup);
    smoother_->setEnabled(settings_.smoothEnabled); smoother_->setTau(settings_.smoothTau);
    smoother_->setCurrent(currentColor_); pushEffectSettings(); pushAmbilightSettings(); setMode(settings_.mode, false);
    refreshScreens(); updatePaletteUi(); updateAmbilightSummary(); refreshTrayIcon(true);
    QTimer::singleShot(0, wifi_, &WifiManager::start);
}

MainWindow::~MainWindow() { if (qApp && shutdownFilter_) qApp->removeNativeEventFilter(shutdownFilter_); effects_->stop(); ambilight_->stop(); wifi_->stop(); }

QWidget* MainWindow::createControlTab()
{
    auto* w = new QWidget(tabs_); auto* v = new QVBoxLayout(w); v->setContentsMargins(20,20,20,20); v->setSpacing(14);
    colorPreview_ = new QLabel(w); colorPreview_->setFixedHeight(88); colorPreview_->setAlignment(Qt::AlignCenter); colorPreview_->setAutoFillBackground(true); v->addWidget(colorPreview_);
    auto* wr = new QHBoxLayout; wr->addStretch(); colorWheel_ = new ColorWheel(w); colorWheel_->setFixedSize(250,250); wr->addWidget(colorWheel_); wr->addStretch(); v->addLayout(wr);
    hexLabel_ = new QLabel(QStringLiteral("#000000"), w); hexLabel_->setAlignment(Qt::AlignCenter); hexLabel_->setProperty("role", "hex"); v->addWidget(hexLabel_);
    auto* b = card(w); auto* bl = new QHBoxLayout(b); bl->addWidget(new QLabel(tr("Brightness"), b)); brightnessSlider_ = new QSlider(Qt::Horizontal,b); brightnessSlider_->setRange(1,100); bl->addWidget(brightnessSlider_,1); brightnessValue_=valueLabel("100%",b); bl->addWidget(brightnessValue_); v->addWidget(b);
    auto* m=card(w); auto* ml=new QHBoxLayout(m); ml->addWidget(new QLabel(tr("Source"),m)); modeCombo_=new QComboBox(m); modeCombo_->addItem(tr("Effect"),"effect"); modeCombo_->addItem(tr("Ambilight"),"ambilight"); ml->addWidget(modeCombo_,1); v->addWidget(m);
    auto* t=card(w); auto* tl=new QHBoxLayout(t); tl->addWidget(new QLabel(tr("Smoothness"),t)); smoothTestButton_=new QPushButton(tr("▶  Smoothness test"),t); tl->addWidget(smoothTestButton_,1); v->addWidget(t);
    auto* ph=new QHBoxLayout; auto* on=new QPushButton(tr("Turn on"),w); on->setProperty("accent",true); on->setMinimumHeight(44); auto* off=new QPushButton(tr("Turn off"),w); off->setProperty("danger",true); off->setMinimumHeight(44); ph->addWidget(on); ph->addWidget(off); v->addLayout(ph); v->addStretch();
    connect(on,&QPushButton::clicked,this,[this]{setPower(true);}); connect(off,&QPushButton::clicked,this,[this]{setPower(false);}); return w;
}

QWidget* MainWindow::createEffectsTab()
{
    QWidget* body=nullptr; auto* scroll=scrollPage(body,tabs_); auto* v=new QVBoxLayout(body); v->setContentsMargins(20,20,20,20); v->setSpacing(14);
    auto* g=new QGroupBox(tr("Effect mode"),body); auto* f=new QFormLayout(g); effectCombo_=new QComboBox(g);
    for(const QString& e:EffectEngine::effects()) effectCombo_->addItem(EffectEngine::labels().value(e),e); f->addRow(tr("Effect:"),effectCombo_);
    effectDescription_=new QLabel(g); effectDescription_->setWordWrap(true); effectDescription_->setProperty("role","desc"); f->addRow(QString(),effectDescription_);
    auto sliderRow=[&](QSlider*& s,QLabel*& l,int lo,int hi,int val){ auto* row=new QHBoxLayout; s=new QSlider(Qt::Horizontal,g); s->setRange(lo,hi); s->setValue(val); l=valueLabel(QString(),g); row->addWidget(s,1); row->addWidget(l); return row; };
    f->addRow(tr("Speed:"),sliderRow(speedSlider_,speedValue_,5,500,100)); reverseCheck_=new QCheckBox(tr("Reverse direction"),g); f->addRow(QString(),reverseCheck_); v->addWidget(g);
    auto* p=new QGroupBox(tr("Palette"),body); auto* pv=new QVBoxLayout(p); auto* info=new QLabel(tr("Color 1 is the main color. Effects use from one to all active colors."),p); info->setWordWrap(true); info->setProperty("role","info"); pv->addWidget(info);
    auto* pw=new QHBoxLayout; pw->addStretch(); paletteWheel_=new ColorWheel(p); paletteWheel_->setVisualFollowsInput(true); paletteWheel_->setFixedSize(220,220); pw->addWidget(paletteWheel_); pw->addStretch(); pv->addLayout(pw); paletteActiveLabel_=new QLabel(p); paletteActiveLabel_->setAlignment(Qt::AlignCenter); paletteActiveLabel_->setProperty("role","hex"); pv->addWidget(paletteActiveLabel_);
    auto* slots=new QHBoxLayout; for(int i=0;i<4;++i){auto* btn=new QPushButton(p);btn->setFixedSize(52,52);btn->setProperty("slot",true);paletteButtons_.append(btn);slots->addWidget(btn);connect(btn,&QPushButton::clicked,this,[this,i]{selectPaletteSlot(i);});} slots->addStretch(); pv->addLayout(slots);
    auto* controls=new QHBoxLayout; paletteMinus_=new QPushButton(tr("− Remove"),p); palettePlus_=new QPushButton(tr("+ Add"),p); controls->addWidget(paletteMinus_);controls->addWidget(palettePlus_);controls->addStretch();pv->addLayout(controls);paletteUsageLabel_=new QLabel(p);paletteUsageLabel_->setProperty("role","info");pv->addWidget(paletteUsageLabel_);v->addWidget(p);
    auto* params=new QGroupBox(tr("Parameters"),body);auto* pf=new QFormLayout(params);pf->addRow(tr("Intensity:"),sliderRow(intensitySlider_,intensityValue_,10,100,100));pf->addRow(tr("Noise:"),sliderRow(noiseSlider_,noiseValue_,0,100,0));v->addWidget(params);v->addStretch();return scroll;
}

QWidget* MainWindow::createAmbilightTab()
{
    auto* w=new QWidget(tabs_);auto* v=new QVBoxLayout(w);v->setContentsMargins(20,20,20,20);v->setSpacing(14);
    auto* g=new QGroupBox(tr("Monitor"),w);auto* gl=new QVBoxLayout(g);auto* row=new QHBoxLayout;screenCombo_=new QComboBox(g);screenRefreshButton_=new QPushButton(tr("Refresh"),g);row->addWidget(screenCombo_,1);row->addWidget(screenRefreshButton_);gl->addLayout(row);v->addWidget(g);
    auto* cg=new QGroupBox(tr("Screen capture"),w);auto* cl=new QVBoxLayout(cg);ambiCaptureCombo_=new QComboBox(w);
    for(const QString& key:{QStringLiteral("auto"),QStringLiteral("dxgi"),QStringLiteral("gdi"),QStringLiteral("wgc")})
        ambiCaptureCombo_->addItem(AmbiLight::captureModeName(AmbiLight::captureModeFromString(key)),key);
    cl->addWidget(ambiCaptureCombo_);v->addWidget(cg);
    ambilightSummary_=new QLabel(w);ambilightSummary_->setWordWrap(true);ambilightSummary_->setProperty("role","info");v->addWidget(ambilightSummary_);auto* edit=new QPushButton(tr("Open area and parameters editor…"),w);edit->setMinimumHeight(44);v->addWidget(edit);
    ambilightState_=new QLabel(tr("Ambilight off"),w);ambilightState_->setAlignment(Qt::AlignCenter);ambilightState_->setProperty("role","status");v->addWidget(ambilightState_);auto* row2=new QHBoxLayout;auto* on=new QPushButton(tr("Turn on Ambilight"),w);on->setProperty("accent",true);auto* off=new QPushButton(tr("Turn off"),w);off->setProperty("danger",true);row2->addWidget(on);row2->addWidget(off);v->addLayout(row2);v->addStretch();
    connect(edit,&QPushButton::clicked,this,[this]{
        if(!ambiEditor_){
            ambiEditor_=new AmbiEditor(ambilight_,this);
            connect(ambiEditor_,&AmbiEditor::settingsApplied,this,[this](const QVariantMap& m){
                settings_.ambiRegion=m.value("ambi_region").toString(); settings_.ambiBand=m.value("ambi_band").toInt();
                settings_.ambiBoost=m.value("ambi_boost").toDouble(); settings_.ambiSmooth=m.value("ambi_smooth").toDouble();
                settings_.ambiMin=m.value("ambi_min").toInt(); settings_.ambiAuto=m.value("ambi_auto").toBool();
                settings_.ambiFreq=m.value("ambi_freq").toDouble(); settings_.ambiScreen=m.value("ambi_screen").toInt();
                settings_.ambiCombine=m.value("ambi_combine").toString(); settings_.ambiRect=m.value("ambi_rect").toRectF();
                settings_.ambiCapture=m.value("ambi_capture").toString();
                pushAmbilightSettings(); refreshScreens(); updateAmbilightSummary(); store_->save(settings_);
            });
        }
        QVariantMap m; m["ambi_region"]=settings_.ambiRegion;m["ambi_band"]=settings_.ambiBand;m["ambi_boost"]=settings_.ambiBoost;m["ambi_smooth"]=settings_.ambiSmooth;m["ambi_min"]=settings_.ambiMin;m["ambi_auto"]=settings_.ambiAuto;m["ambi_freq"]=settings_.ambiFreq;m["ambi_screen"]=settings_.ambiScreen;m["ambi_combine"]=settings_.ambiCombine;m["ambi_rect"]=settings_.ambiRect;m["ambi_capture"]=settings_.ambiCapture;
        ambiEditor_->setSettings(m);ambiEditor_->show();ambiEditor_->raise();
    });
    connect(on,&QPushButton::clicked,this,[this]{setMode("ambilight");});connect(off,&QPushButton::clicked,this,[this]{setMode("effect");});return w;
}

QWidget* MainWindow::createSettingsTab()
{
    QWidget* body=nullptr;auto* scroll=scrollPage(body,tabs_);auto* v=new QVBoxLayout(body);v->setContentsMargins(20,20,20,20);v->setSpacing(14);
    auto* dev=new QGroupBox(tr("Device"),body);auto* dv=new QVBoxLayout(dev);auto* mr=new QHBoxLayout;addressEdit_=new QLineEdit(dev);applyAddressButton_=new QPushButton(tr("Apply"),dev);mr->addWidget(addressEdit_,1);mr->addWidget(applyAddressButton_);dv->addLayout(mr);scanButton_=new QPushButton(tr("🔍  Find Magic Home devices"),dev);dv->addWidget(scanButton_);deviceList_=new QListWidget(dev);deviceList_->setMinimumHeight(100);dv->addWidget(deviceList_);useDeviceButton_=new QPushButton(tr("Use selected"),dev);dv->addWidget(useDeviceButton_);v->addWidget(dev);
    auto makeSlider=[&](QFormLayout* f,const QString& name,QSlider*& s,QLabel*& l,int lo,int hi){auto* r=new QHBoxLayout;s=new QSlider(Qt::Horizontal,body);s->setRange(lo,hi);l=valueLabel(QString(),body);r->addWidget(s,1);r->addWidget(l);f->addRow(name,r);};
    auto* bg=new QGroupBox(tr("Wi-Fi (TCP)"),body);auto* bf=new QFormLayout(bg);makeSlider(bf,tr("Interval:"),netIntervalSlider_,netIntervalValue_,0,200);makeSlider(bf,tr("Deduplication:"),netDedupSlider_,netDedupValue_,0,500);v->addWidget(bg);
    auto* sg=new QGroupBox(tr("Smoothing"),body);auto* sf=new QFormLayout(sg);smoothingCheck_=new QCheckBox(tr("Enable smooth transitions"),sg);sf->addRow(smoothingCheck_);makeSlider(sf,tr("Transition time:"),tauSlider_,tauValue_,20,500);instantColorCheck_=new QCheckBox(tr("Instant color response"),sg);sf->addRow(instantColorCheck_);v->addWidget(sg);
    auto* lg=new QGroupBox(tr("Logging"),body);auto* ll=new QVBoxLayout(lg);loggingCheck_=new QCheckBox(tr("Write log file"),lg);ll->addWidget(loggingCheck_);auto* lr=new QHBoxLayout;openLogButton_=new QPushButton(tr("Open log"),lg);clearLogButton_=new QPushButton(tr("Clear buffer"),lg);lr->addWidget(openLogButton_);lr->addWidget(clearLogButton_);ll->addLayout(lr);v->addWidget(lg);
    auto* lang=new QGroupBox(tr("Language"),body);auto* langForm=new QFormLayout(lang);languageCombo_=new QComboBox(lang);languageCombo_->addItem(QStringLiteral("English"),QStringLiteral("en"));languageCombo_->addItem(QStringLiteral("Русский"),QStringLiteral("ru"));langForm->addRow(tr("Interface language:"),languageCombo_);v->addWidget(lang);
    auto* behavior=new QGroupBox(tr("Behavior"),body);auto* bv=new QVBoxLayout(behavior);autostartCheck_=new QCheckBox(tr("Start with Windows"),behavior);minimizedCheck_=new QCheckBox(tr("Start minimized"),behavior);saveOnExitCheck_=new QCheckBox(tr("Save settings on exit"),behavior);keepaliveCheck_=new QCheckBox(tr("Keepalive for static color"),behavior);restorePowerCheck_=new QCheckBox(tr("Restore powered-on state on startup"),behavior);powerOffOnExitCheck_=new QCheckBox(tr("Turn the strip off on exit"),behavior);powerOffOnShutdownCheck_=new QCheckBox(tr("Turn the strip off when Windows shuts down"),behavior);bv->addWidget(autostartCheck_);bv->addWidget(minimizedCheck_);bv->addWidget(saveOnExitCheck_);bv->addWidget(keepaliveCheck_);bv->addWidget(restorePowerCheck_);bv->addWidget(powerOffOnExitCheck_);bv->addWidget(powerOffOnShutdownCheck_);v->addWidget(behavior);auto* save=new QPushButton(tr("Save settings"),body);save->setProperty("accent",true);save->setMinimumHeight(44);v->addWidget(save);v->addStretch();connect(save,&QPushButton::clicked,this,&MainWindow::saveSettings);return scroll;
}

void MainWindow::connectSignals()
{
    connect(effects_,&EffectEngine::colorChanged,this,&MainWindow::submitGeneratedColor);connect(ambilight_,&AmbiLight::colorChanged,this,&MainWindow::submitGeneratedColor);connect(smoother_,&ColorSmoother::colorChanged,this,&MainWindow::onSmoothedColor);
    connect(colorWheel_,&ColorWheel::colorChanged,this,&MainWindow::setStaticColor);connect(brightnessSlider_,&QSlider::valueChanged,this,[this](int v){settings_.brightness=v;brightnessValue_->setText(QStringLiteral("%1%").arg(v));if(settings_.effect=="static")submitGeneratedColor(settings_.color.red(),settings_.color.green(),settings_.color.blue());});
    connect(modeCombo_,&QComboBox::currentIndexChanged,this,[this]{if(!updating_)setMode(modeCombo_->currentData().toString());});connect(smoothTestButton_,&QPushButton::clicked,this,&MainWindow::runSmoothTest);
    connect(effectCombo_,&QComboBox::currentIndexChanged,this,[this]{if(!updating_)setEffect(effectCombo_->currentData().toString());});connect(speedSlider_,&QSlider::valueChanged,this,[this](int v){settings_.effectSpeed=v/100.0;speedValue_->setText(QStringLiteral("%1x").arg(settings_.effectSpeed,0,'f',2));pushEffectSettings();if(settings_.mode=="effect"&&protocol::usesBuiltinPattern(settings_.effect))applyEffectState();});connect(intensitySlider_,&QSlider::valueChanged,this,[this](int v){settings_.effectIntensity=v/100.0;intensityValue_->setText(QStringLiteral("%1%").arg(v));pushEffectSettings();});connect(noiseSlider_,&QSlider::valueChanged,this,[this](int v){settings_.effectNoise=v/100.0;noiseValue_->setText(QStringLiteral("%1%").arg(v));pushEffectSettings();});connect(reverseCheck_,&QCheckBox::toggled,this,[this](bool v){settings_.effectReverse=v;pushEffectSettings();});connect(paletteWheel_,&ColorWheel::colorChanged,this,[this](int r,int g,int b){if(updating_)return;settings_.palette[paletteSlot_]=QColor(r,g,b);if(settings_.mode=="ambilight")setMode("effect",false);if(paletteSlot_==0){settings_.color=QColor(r,g,b);colorWheel_->setRgb(r,g,b);if(settings_.effect=="static")submitGeneratedColor(r,g,b);}pushEffectSettings();updatePaletteUi();});connect(palettePlus_,&QPushButton::clicked,this,[this]{settings_.paletteCount=std::min(4,settings_.paletteCount+1);paletteSlot_=settings_.paletteCount-1;updatePaletteUi();pushEffectSettings();});connect(paletteMinus_,&QPushButton::clicked,this,[this]{settings_.paletteCount=std::max(2,settings_.paletteCount-1);paletteSlot_=std::min(paletteSlot_,settings_.paletteCount-1);updatePaletteUi();pushEffectSettings();});
    connect(screenRefreshButton_,&QPushButton::clicked,this,&MainWindow::refreshScreens);connect(screenCombo_,&QComboBox::currentIndexChanged,this,[this](int i){if(i>=0){settings_.ambiScreen=screenCombo_->currentData().toInt();pushAmbilightSettings();updateAmbilightSummary();}});
    connect(ambiCaptureCombo_,&QComboBox::currentIndexChanged,this,[this](int){if(updating_)return;settings_.ambiCapture=ambiCaptureCombo_->currentData().toString();pushAmbilightSettings();updateAmbilightSummary();store_->save(settings_);});
    connect(applyAddressButton_,&QPushButton::clicked,this,[this]{settings_.address=addressEdit_->text().trimmed();wifi_->setAddress(settings_.address);wifi_->start();setStatus(tr("Address: %1").arg(settings_.address));});connect(scanButton_,&QPushButton::clicked,scanner_,&DeviceScanner::start);connect(scanner_,&DeviceScanner::scanStarted,this,[this]{deviceList_->clear();deviceList_->addItem(tr("Searching for devices…"));scanButton_->setEnabled(false);});connect(scanner_,&DeviceScanner::deviceFound,this,[this](const QString&a,const QString&n){if(deviceList_->count()==1&&!deviceList_->item(0)->data(Qt::UserRole).isValid())deviceList_->clear();auto* item=new QListWidgetItem(QStringLiteral("%1 — %2").arg(n,a),deviceList_);item->setData(Qt::UserRole,a);settings_.lastDevices.removeAll(a);settings_.lastDevices.prepend(a);while(settings_.lastDevices.size()>5)settings_.lastDevices.removeLast();store_->save(settings_);});connect(scanner_,&DeviceScanner::finished,this,[this]{scanButton_->setEnabled(true);if(deviceList_->count()==0)deviceList_->addItem(tr("No devices found"));setStatus(tr("Scanning finished"));});connect(scanner_,&DeviceScanner::error,this,[this](const QString&e){scanButton_->setEnabled(true);setStatus(tr("Scan error: %1").arg(e));});auto useDevice=[this]{auto*i=deviceList_->currentItem();if(i&&i->data(Qt::UserRole).isValid()){addressEdit_->setText(i->data(Qt::UserRole).toString());applyAddressButton_->click();}};connect(useDeviceButton_,&QPushButton::clicked,this,useDevice);connect(deviceList_,&QListWidget::itemDoubleClicked,this,[useDevice](QListWidgetItem*){useDevice();});
    connect(netIntervalSlider_,&QSlider::valueChanged,this,[this](int v){settings_.netInterval=v/100.0;netIntervalValue_->setText(QStringLiteral("%1 с").arg(settings_.netInterval,0,'f',2));wifi_->setMinimumInterval(settings_.netInterval);});connect(netDedupSlider_,&QSlider::valueChanged,this,[this](int v){settings_.netDedup=v/100.0;netDedupValue_->setText(QStringLiteral("%1 с").arg(settings_.netDedup,0,'f',2));wifi_->setDeduplicationWindow(settings_.netDedup);});connect(smoothingCheck_,&QCheckBox::toggled,this,[this](bool v){settings_.smoothEnabled=v;smoother_->setEnabled(v);tauSlider_->setEnabled(v);});connect(instantColorCheck_,&QCheckBox::toggled,this,[this](bool v){settings_.instantColor=v;colorWheel_->setEmitThrottleEnabled(!v);paletteWheel_->setEmitThrottleEnabled(!v);});connect(tauSlider_,&QSlider::valueChanged,this,[this](int v){settings_.smoothTau=v;tauValue_->setText(QStringLiteral("%1 мс").arg(v));smoother_->setTau(v);});connect(loggingCheck_,&QCheckBox::toggled,this,[this](bool v){settings_.loggingEnabled=v;AppLog::setEnabled(v);});connect(powerOffOnShutdownCheck_,&QCheckBox::toggled,this,[this](bool v){settings_.powerOffOnShutdown=v;if(powerOffOnShutdownAction_){QSignalBlocker b(powerOffOnShutdownAction_);powerOffOnShutdownAction_->setChecked(v);}});connect(openLogButton_,&QPushButton::clicked,this,&MainWindow::openLogWindow);connect(clearLogButton_,&QPushButton::clicked,this,[]{AppLog::clear();});connect(languageCombo_,&QComboBox::currentIndexChanged,this,[this](int){if(updating_)return;const QString code=languageCombo_->currentData().toString();if(code==settings_.language)return;settings_.language=code;store_->save(settings_);QProcess::startDetached(QCoreApplication::applicationFilePath());quitApplication();});
    connect(wifi_,&WifiManager::statusChanged,this,&MainWindow::setStatus);connect(wifi_,&WifiManager::connectedChanged,this,[this](bool c){const bool reconnect=c&&!connected_;connected_=c;connectionDot_->setStyleSheet(c?"color:#22c55e":"color:#ef4444");connectionDot_->setToolTip(c?tr("Wi-Fi: connected"):tr("Wi-Fi: disconnected"));if(reconnect){if(settings_.restorePower)powerOn_=settings_.lastPowerOn;wifi_->send(powerOn_?protocol::powerOnCommand():protocol::powerOffCommand());if(powerOn_&&settings_.mode!="ambilight")QTimer::singleShot(150,this,[this]{applyEffectState();});}});
    connect(store_,&SettingsStore::error,this,&MainWindow::setStatus);
    connect(popup_,&TrayPopup::colorChanged,this,&MainWindow::setStaticColor);connect(popup_,&TrayPopup::brightnessChanged,brightnessSlider_,&QSlider::setValue);connect(popup_,&TrayPopup::smoothingTauChanged,tauSlider_,&QSlider::setValue);connect(popup_,&TrayPopup::modeChanged,this,[this](const QString&m){setMode(m);});connect(popup_,&TrayPopup::effectChanged,this,&MainWindow::setEffect);connect(popup_,&TrayPopup::powerRequested,this,&MainWindow::setPower);connect(popup_,&TrayPopup::settingsRequested,this,[this]{tabs_->setCurrentIndex(3);showWindow();});connect(popup_,&TrayPopup::windowRequested,this,&MainWindow::showWindow);
}

void MainWindow::applySettingsToUi()
{
    updating_=true;addressEdit_->setText(settings_.address);brightnessSlider_->setValue(settings_.brightness);brightnessValue_->setText(QStringLiteral("%1%").arg(settings_.brightness));modeCombo_->setCurrentIndex(modeCombo_->findData(settings_.mode));effectCombo_->setCurrentIndex(effectCombo_->findData(settings_.effect));speedSlider_->setValue(qRound(settings_.effectSpeed*100));intensitySlider_->setValue(qRound(settings_.effectIntensity*100));noiseSlider_->setValue(qRound(settings_.effectNoise*100));reverseCheck_->setChecked(settings_.effectReverse);colorWheel_->setRgb(settings_.color.red(),settings_.color.green(),settings_.color.blue());paletteWheel_->setRgb(settings_.palette[0].red(),settings_.palette[0].green(),settings_.palette[0].blue());netIntervalSlider_->setValue(qRound(settings_.netInterval*100));netIntervalValue_->setText(QStringLiteral("%1 с").arg(settings_.netInterval,0,'f',2));netDedupSlider_->setValue(qRound(settings_.netDedup*100));netDedupValue_->setText(QStringLiteral("%1 с").arg(settings_.netDedup,0,'f',2));smoothingCheck_->setChecked(settings_.smoothEnabled);tauSlider_->setValue(settings_.smoothTau);tauValue_->setText(QStringLiteral("%1 мс").arg(settings_.smoothTau));tauSlider_->setEnabled(settings_.smoothEnabled);instantColorCheck_->setChecked(settings_.instantColor);colorWheel_->setEmitThrottleEnabled(!settings_.instantColor);paletteWheel_->setEmitThrottleEnabled(!settings_.instantColor);loggingCheck_->setChecked(settings_.loggingEnabled);autostartCheck_->setChecked(settings_.autostart);minimizedCheck_->setChecked(settings_.startMinimized);saveOnExitCheck_->setChecked(settings_.saveOnExit);keepaliveCheck_->setChecked(settings_.keepalive);restorePowerCheck_->setChecked(settings_.restorePower);powerOffOnExitCheck_->setChecked(settings_.powerOffOnExit);powerOffOnShutdownCheck_->setChecked(settings_.powerOffOnShutdown);languageCombo_->setCurrentIndex(languageCombo_->findData(settings_.language));
    deviceList_->clear();
    for (const QString& addr : settings_.lastDevices) {
        auto* item = new QListWidgetItem(addr, deviceList_);
        item->setData(Qt::UserRole, addr);
    }
    { QSignalBlocker b(ambiCaptureCombo_); ambiCaptureCombo_->setCurrentIndex(ambiCaptureCombo_->findData(settings_.ambiCapture)); }
    updating_=false;updatePreview(settings_.color);
}

void MainWindow::collectSettingsFromUi(){settings_.address=addressEdit_->text().trimmed();settings_.language=languageCombo_->currentData().toString();settings_.autostart=autostartCheck_->isChecked();settings_.startMinimized=minimizedCheck_->isChecked();settings_.saveOnExit=saveOnExitCheck_->isChecked();settings_.keepalive=keepaliveCheck_->isChecked();settings_.restorePower=restorePowerCheck_->isChecked();settings_.powerOffOnExit=powerOffOnExitCheck_->isChecked();settings_.powerOffOnShutdown=powerOffOnShutdownCheck_->isChecked();settings_.loggingEnabled=loggingCheck_->isChecked();}
void MainWindow::saveSettings(){collectSettingsFromUi();if(store_->save(settings_))setStatus(tr("Settings saved"));}
void MainWindow::setStatus(const QString& text){statusLabel_->setText(text);AppLog::logline(text);}
void MainWindow::setMode(const QString& mode,bool immediate){settings_.mode=(mode=="ambilight"?"ambilight":"effect");{QSignalBlocker b(modeCombo_);modeCombo_->setCurrentIndex(modeCombo_->findData(settings_.mode));}if(settings_.mode=="ambilight"){effects_->stop();ambilight_->start();ambilightState_->setText(tr("Ambilight on"));}else{ambilight_->stop();pushEffectSettings();if(protocol::usesBuiltinPattern(settings_.effect)){effects_->stop();applyEffectState();}else{effects_->start();}ambilightState_->setText(tr("Ambilight off"));}if(immediate)setStatus(settings_.mode=="ambilight"?tr("Source: Ambilight"):tr("Source: effects"));}
void MainWindow::setEffect(const QString& effect){if(!EffectEngine::effects().contains(effect))return;settings_.effect=effect;{QSignalBlocker b(effectCombo_);effectCombo_->setCurrentIndex(effectCombo_->findData(effect));}effectDescription_->setText(EffectEngine::descriptions().value(effect));updatePaletteUi();pushEffectSettings();setMode("effect",false);}
void MainWindow::applyEffectState(){if(!connected_||!powerOn_||settings_.mode=="ambilight")return;const int code=protocol::builtinPatternCode(settings_.effect);if(code>=0){wifi_->send(protocol::builtinPatternCommand(code,protocol::deviceSpeed(settings_.effectSpeed)));return;}submitGeneratedColor(settings_.color.red(),settings_.color.green(),settings_.color.blue());}
void MainWindow::pushEffectSettings(){QList<QColor> p;for(int i=0;i<settings_.paletteCount;++i)p<<settings_.palette[i];effects_->setEffect(settings_.effect);effects_->setPalette(p);effects_->setSpeed(settings_.effectSpeed);effects_->setIntensity(settings_.effectIntensity);effects_->setNoise(settings_.effectNoise);effects_->setReverse(settings_.effectReverse);speedValue_->setText(QStringLiteral("%1x").arg(settings_.effectSpeed,0,'f',2));intensityValue_->setText(QStringLiteral("%1%").arg(qRound(settings_.effectIntensity*100)));noiseValue_->setText(QStringLiteral("%1%").arg(qRound(settings_.effectNoise*100)));effectDescription_->setText(EffectEngine::descriptions().value(settings_.effect));}
void MainWindow::pushAmbilightSettings(){ambilight_->setRegion(settings_.ambiRegion);ambilight_->setBandPct(settings_.ambiBand);ambilight_->setBoost(settings_.ambiBoost);ambilight_->setSmooth(settings_.ambiSmooth);ambilight_->setMinLevel(settings_.ambiMin);ambilight_->setAutoBright(settings_.ambiAuto);ambilight_->setFrequency(settings_.ambiFreq);ambilight_->setScreenIndex(settings_.ambiScreen);ambilight_->setCombine(settings_.ambiCombine);ambilight_->setCustomRect(settings_.ambiRect);ambilight_->setCaptureMode(AmbiLight::captureModeFromString(settings_.ambiCapture));}
void MainWindow::setPower(bool enabled){powerOn_=enabled;settings_.lastPowerOn=enabled;store_->setLastPowerOn(enabled);if(enabled){wifi_->send(protocol::powerOnCommand());if(settings_.mode!="ambilight")QTimer::singleShot(120,this,[this]{applyEffectState();});setStatus(tr("Backlight on"));}else{wifi_->send(protocol::powerOffCommand());smoother_->setCurrent(QColor(0,0,0));currentColor_=QColor(0,0,0);updatePreview(currentColor_);setStatus(tr("Backlight off"));}}
void MainWindow::setPowerOffOnShutdown(bool enabled){if(settings_.powerOffOnShutdown==enabled)return;settings_.powerOffOnShutdown=enabled;{QSignalBlocker b(powerOffOnShutdownCheck_);powerOffOnShutdownCheck_->setChecked(enabled);}store_->save(settings_);setStatus(enabled?tr("The strip will be turned off when Windows shuts down"):tr("The strip state will be kept when Windows shuts down"));}
void MainWindow::sendShutdownPowerOff(){if(!settings_.powerOffOnShutdown||!powerOn_||!wifi_)return;wifi_->sendUrgent(protocol::powerOffCommand());}
void MainWindow::submitGeneratedColor(int r,int g,int b){if(!powerOn_||!connected_)return;double k=settings_.brightness/100.0;smoother_->animateTo(qRound(r*k),qRound(g*k),qRound(b*k));}
void MainWindow::onSmoothedColor(int r,int g,int b){if(!powerOn_||!connected_)return;currentColor_=QColor(r,g,b);if(settings_.effect=="static"&&colorWheel_)colorWheel_->setVisualRgb(r,g,b);updatePreview(currentColor_);wifi_->send(protocol::colorCommand(r,g,b));if(!iconTimer_->isActive())iconTimer_->start();if(popup_->isVisible())popup_->syncState(r,g,b,settings_.brightness,settings_.mode,settings_.effect,settings_.smoothTau);}
void MainWindow::setStaticColor(int r,int g,int b){if(updating_)return;const bool wasStaticEffect=settings_.mode=="effect"&&settings_.effect=="static";if(!wasStaticEffect){ambilight_->stop();settings_.mode="effect";settings_.effect="static";paletteSlot_=0;updating_=true;modeCombo_->setCurrentIndex(modeCombo_->findData(settings_.mode));effectCombo_->setCurrentIndex(effectCombo_->findData(settings_.effect));paletteWheel_->setRgb(r,g,b);updating_=false;effects_->setEffect("static");pushEffectSettings();effects_->start();updatePaletteUi();}settings_.color=QColor(r,g,b);settings_.palette[0]=settings_.color;effects_->setPalette({settings_.color});submitGeneratedColor(r,g,b);}
void MainWindow::updatePreview(const QColor& c){hexLabel_->setText(c.name().toUpper());QPalette palette=colorPreview_->palette();palette.setColor(QPalette::Window,c);colorPreview_->setPalette(palette);}
void MainWindow::selectPaletteSlot(int i){if(i<0||i>=settings_.paletteCount)return;paletteSlot_=i;updating_=true;paletteWheel_->setRgb(settings_.palette[i].red(),settings_.palette[i].green(),settings_.palette[i].blue());updating_=false;updatePaletteUi();}
void MainWindow::updatePaletteUi(){int usage=EffectEngine::paletteUsage().value(settings_.effect,-1);int used=usage<0?settings_.paletteCount:std::min(usage,settings_.paletteCount);for(int i=0;i<4;++i){auto*b=paletteButtons_[i];const bool active=i<settings_.paletteCount;const bool effectUses=active&&i<used;b->setVisible(active);b->setEnabled(effectUses);b->setText(active&&!effectUses?QStringLiteral("×"):QString());b->setProperty("slotActive",i==paletteSlot_&&effectUses);const QColor c=settings_.palette[i];const QString background=effectUses?c.name():QStringLiteral("rgba(%1,%2,%3,90)").arg(c.red()).arg(c.green()).arg(c.blue());b->setStyleSheet(QStringLiteral("QPushButton{background:%1;color:rgba(255,255,255,190);font-size:30px;font-weight:700;border:%2px solid %3;border-radius:10px}QPushButton:disabled{color:rgba(255,255,255,190)}").arg(background).arg(i==paletteSlot_&&effectUses?3:2).arg(i==paletteSlot_&&effectUses?"#b070ff":"#3a3a48"));}paletteActiveLabel_->setText(paletteSlot_==0?tr("Editing: Color 1 (primary)"):tr("Editing: Color %1").arg(paletteSlot_+1));paletteUsageLabel_->setText(usage==0?tr("Rainbow does not use the palette."):tr("Active colors: %1. Current effect uses: %2.").arg(settings_.paletteCount).arg(used));palettePlus_->setEnabled(settings_.paletteCount<4);paletteMinus_->setEnabled(settings_.paletteCount>2);refreshTrayIcon(true);}
void MainWindow::refreshScreens(){int previous=settings_.ambiScreen;screenCombo_->clear();for(const auto&s:AmbiLight::listScreens())screenCombo_->addItem(QStringLiteral("%1 — %2×%3%4").arg(s.name).arg(s.size.width()).arg(s.size.height()).arg(s.primary?tr(" (primary)"):QString()),s.index);int i=screenCombo_->findData(previous);screenCombo_->setCurrentIndex(i>=0?i:0);}
void MainWindow::updateAmbilightSummary(){ambilightSummary_->setText(tr("Area: %1\nBand: %2% · boost: %3x · smoothing: %4\nFrequency: %5 Hz · combining: %6\nCapture: %7").arg(AmbiLight::regions().value(settings_.ambiRegion,settings_.ambiRegion)).arg(settings_.ambiBand).arg(settings_.ambiBoost,0,'f',1).arg(settings_.ambiSmooth,0,'f',2).arg(settings_.ambiFreq,0,'f',1).arg(AmbiLight::combineModes().value(settings_.ambiCombine,settings_.ambiCombine)).arg(AmbiLight::captureModeName(AmbiLight::captureModeFromString(settings_.ambiCapture))));}
void MainWindow::runSmoothTest(){smoothTestPrevMode_=settings_.mode;smoothTestPrevEffect_=settings_.effect;setEffect("static");effects_->stop();smoothTestColors_.clear();for(int i=0;i<settings_.paletteCount;++i)smoothTestColors_<<settings_.palette[i];for(int i=0;i<8;++i)smoothTestColors_<<QColor::fromHsv(i*45,255,255);smoothTestColors_<<settings_.palette[0];smoothTestIndex_=0;smoothTestTimer_->start();smoothTestStep();}
void MainWindow::smoothTestStep(){if(smoothTestIndex_>=smoothTestColors_.size()){smoothTestTimer_->stop();effects_->stop();if(smoothTestPrevMode_=="ambilight"){setMode("ambilight",false);}else{setEffect(smoothTestPrevEffect_);}setStatus(tr("Smoothness test finished"));return;}const QColor c=smoothTestColors_[smoothTestIndex_++];submitGeneratedColor(c.red(),c.green(),c.blue());setStatus(tr("Smoothness test… (%1/%2)").arg(smoothTestIndex_).arg(smoothTestColors_.size()));}

void MainWindow::setupTray(){tray_=new QSystemTrayIcon(IconFactory::appIcon(64),this);tray_->setToolTip(QString::fromLatin1(config::ApplicationName));auto* menu=new QMenu(this);menu->addAction(tr("Open window"),this,&MainWindow::showWindow);menu->addAction(tr("🎨 Quick settings"),this,&MainWindow::showPopup);menu->addSeparator();menu->addAction(tr("Turn backlight on"),this,[this]{setPower(true);});menu->addAction(tr("Turn backlight off"),this,[this]{setPower(false);});powerOffOnShutdownAction_=menu->addAction(tr("Turn off when Windows shuts down"));powerOffOnShutdownAction_->setCheckable(true);powerOffOnShutdownAction_->setChecked(settings_.powerOffOnShutdown);connect(powerOffOnShutdownAction_,&QAction::toggled,this,&MainWindow::setPowerOffOnShutdown);menu->addAction(tr("▶ Smoothness test"),this,&MainWindow::runSmoothTest);menu->addSeparator();menu->addAction(tr("Open log"),this,&MainWindow::openLogWindow);menu->addAction(tr("Settings…"),this,[this]{tabs_->setCurrentIndex(3);showWindow();});menu->addSeparator();menu->addAction(tr("Exit"),this,&MainWindow::quitApplication);tray_->setContextMenu(menu);clickTimer_=new QTimer(this);clickTimer_->setSingleShot(true);clickTimer_->setInterval(QApplication::doubleClickInterval());connect(clickTimer_,&QTimer::timeout,this,&MainWindow::showPopup);connect(tray_,&QSystemTrayIcon::activated,this,[this](QSystemTrayIcon::ActivationReason reason){if(reason==QSystemTrayIcon::Trigger)clickTimer_->start();else if(reason==QSystemTrayIcon::DoubleClick){clickTimer_->stop();showWindow();}});tray_->show();}
void MainWindow::showWindow(){showNormal();raise();activateWindow();}
void MainWindow::showPopup(){if(popup_->isVisible()){popup_->hide();return;}popup_->syncState(currentColor_.red(),currentColor_.green(),currentColor_.blue(),settings_.brightness,settings_.mode,settings_.effect,settings_.smoothTau);popup_->adjustSize();QPoint p=QCursor::pos();QScreen*s=QApplication::screenAt(p);if(!s)s=QApplication::primaryScreen();QRect g=s->availableGeometry();int x=std::clamp(p.x()-popup_->width()/2,g.left()+8,g.right()-popup_->width()-8);int y=p.y()-popup_->height()-12;if(y<g.top()+8)y=p.y()+12;popup_->move(x,y);popup_->show();}
void MainWindow::refreshTrayIcon(bool force){if(settings_.mode=="ambilight"){const QString key=QStringLiteral("ambilight/hsv");if(!force&&key==trayIconKey_)return;trayIconKey_=key;tray_->setIcon(IconFactory::rainbowIcon(64,false));return;}const int redBucket=currentColor_.red()/16;const int greenBucket=currentColor_.green()/16;const int blueBucket=currentColor_.blue()/16;QString key=QStringLiteral("%1/%2/%3/%4/%5").arg(settings_.mode,settings_.effect).arg(redBucket).arg(greenBucket).arg(blueBucket);if(!force&&key==trayIconKey_)return;trayIconKey_=key;tray_->setIcon(IconFactory::trayIcon(currentColor_,settings_.palette.mid(0,settings_.paletteCount),settings_.effect));}
void MainWindow::openLogWindow(){if(!logWindow_)logWindow_=new LogWindow(this);logWindow_->show();logWindow_->raise();logWindow_->activateWindow();}
void MainWindow::closeEvent(QCloseEvent* e){if(!quitting_&&tray_&&tray_->isVisible()){e->ignore();hide();tray_->showMessage(QString::fromLatin1(config::ApplicationName),tr("Application minimized to tray"),QSystemTrayIcon::Information,1500);}else e->accept();}
void MainWindow::quitApplication(){if(quitting_)return;quitting_=true;collectSettingsFromUi();settings_.lastPowerOn=powerOn_;if(settings_.saveOnExit)store_->save(settings_);if(settings_.powerOffOnExit&&powerOn_)wifi_->sendBlocking(protocol::powerOffCommand());effects_->stop();ambilight_->stop();smoother_->setEnabled(false);wifi_->stop();tray_->hide();QApplication::quit();}

} // namespace elkbledom
