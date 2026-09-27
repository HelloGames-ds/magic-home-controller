#include "SystemSessionFilter.h"

#include "AppLog.h"

#include <QByteArray>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wtsapi32.h>
#endif

namespace elkbledom {

SystemSessionFilter::SystemSessionFilter(QObject* parent)
    : QObject(parent)
{
}

SystemSessionFilter::~SystemSessionFilter()
{
#ifdef Q_OS_WIN
    if (watching_ && watchedWindow_ != 0) {
        WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(watchedWindow_));
    }
#endif
}

void SystemSessionFilter::setShutdownHandler(Handler handler)
{
    handler_ = std::move(handler);
}

void SystemSessionFilter::watchWindow(QWidget* window)
{
#ifdef Q_OS_WIN
    if (!window) {
        return;
    }
    const auto handle = reinterpret_cast<HWND>(window->winId());
    if (handle == nullptr) {
        return;
    }
    if (watching_ && watchedWindow_ == WId(handle)) {
        return;
    }
    if (watching_ && watchedWindow_ != 0) {
        WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(watchedWindow_));
    }
    watching_ = WTSRegisterSessionNotification(handle, NOTIFY_FOR_THIS_SESSION) != FALSE;
    watchedWindow_ = WId(handle);
    AppLog::logline(watching_ ? QStringLiteral("Сессия: уведомления о блокировке включены")
                              : QStringLiteral("Сессия: уведомления о блокировке недоступны"));
#endif
}

bool SystemSessionFilter::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType)
    Q_UNUSED(result)
    if (message == nullptr) {
        return false;
    }

    const auto* native = static_cast<MSG*>(message);
    switch (native->message) {
    case WM_QUERYENDSESSION:
    case WM_ENDSESSION:
        if (native->message == WM_ENDSESSION && native->wParam != TRUE) {
            return false;
        }
        if (!shutdownTriggered_) {
            shutdownTriggered_ = true;
            if (handler_) {
                handler_();
            }
        }
        return false;
    case WM_POWERBROADCAST:
        if (native->wParam == PBT_APMSUSPEND) {
            Q_EMIT suspendRequested();
        } else if (native->wParam == PBT_APMRESUMESUSPEND || native->wParam == PBT_APMRESUMEAUTOMATIC) {
            Q_EMIT resumeRequested();
        }
        return false;
    case WM_WTSSESSION_CHANGE:
        if (watchedWindow_ != 0 && native->hwnd != reinterpret_cast<HWND>(watchedWindow_)) {
            return false;
        }
        if (native->wParam == WTS_SESSION_LOCK) {
            Q_EMIT sessionLocked();
        } else if (native->wParam == WTS_SESSION_UNLOCK) {
            Q_EMIT sessionUnlocked();
        }
        return false;
    default:
        return false;
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
    return false;
#endif
}

} // namespace elkbledom
