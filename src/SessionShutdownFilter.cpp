#include "SessionShutdownFilter.h"

#include <QByteArray>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace elkbledom {

SessionShutdownFilter::SessionShutdownFilter(QObject* parent)
    : QObject(parent)
{
}

SessionShutdownFilter::~SessionShutdownFilter() = default;

void SessionShutdownFilter::setHandler(Handler handler)
{
    handler_ = std::move(handler);
}

bool SessionShutdownFilter::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType)
    Q_UNUSED(result)
    if (triggered_ || message == nullptr) {
        return false;
    }

    const auto* native = static_cast<MSG*>(message);
    if (native->message != WM_QUERYENDSESSION && native->message != WM_ENDSESSION) {
        return false;
    }
    if (native->message == WM_ENDSESSION && native->wParam != TRUE) {
        return false;
    }

    triggered_ = true;
    if (handler_) {
        handler_();
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}

} // namespace elkbledom
