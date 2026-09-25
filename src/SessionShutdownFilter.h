#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

#include <functional>

namespace elkbledom {

class SessionShutdownFilter final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    using Handler = std::function<void()>;

    explicit SessionShutdownFilter(QObject* parent = nullptr);
    ~SessionShutdownFilter() override;

    void setHandler(Handler handler);

protected:
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    Handler handler_;
    bool triggered_ = false;
};

} // namespace elkbledom
