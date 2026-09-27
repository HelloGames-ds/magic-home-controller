#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QWidget>

#include <functional>

namespace elkbledom {

class SystemSessionFilter final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    using Handler = std::function<void()>;

    explicit SystemSessionFilter(QObject* parent = nullptr);
    ~SystemSessionFilter() override;

    void setShutdownHandler(Handler handler);
    void watchWindow(QWidget* window);

protected:
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

Q_SIGNALS:
    void sessionLocked();
    void sessionUnlocked();
    void suspendRequested();
    void resumeRequested();

private:
    Handler handler_;
    bool shutdownTriggered_ = false;
    bool watching_ = false;
    WId watchedWindow_ = 0;
};

} // namespace elkbledom
