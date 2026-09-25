#include "WifiWorker.h"

#include "protocol.h"

#include <QDateTime>
#include <QTcpSocket>

namespace elkbledom {
namespace {

constexpr int QueueCapacity = 4;
constexpr int WatchdogIntervalMs = 3000;
constexpr int StalledWriteTimeoutMs = 15000;

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

WifiWorker::WifiWorker(QObject* parent)
    : QObject(parent)
    , address_(QString::fromLatin1(protocol::DefaultAddress))
{
    reconnectTimer_ = new QTimer(this);
    queueTimer_ = new QTimer(this);
    watchdogTimer_ = new QTimer(this);

    reconnectTimer_->setSingleShot(true);
    queueTimer_->setSingleShot(true);
    watchdogTimer_->setInterval(WatchdogIntervalMs);

    connect(reconnectTimer_, &QTimer::timeout, this, &WifiWorker::connectToDevice);
    connect(queueTimer_, &QTimer::timeout, this, &WifiWorker::processQueue);
    connect(watchdogTimer_, &QTimer::timeout, this, &WifiWorker::watchdogTick);
}

WifiWorker::~WifiWorker()
{
    stop();
}

QString WifiWorker::address() const
{
    return address_;
}

bool WifiWorker::isConnected() const noexcept
{
    return connected_;
}

void WifiWorker::start()
{
    if (running_) {
        Q_EMIT statusChanged(connected_
            ? tr("Wi-Fi connected: %1").arg(address_)
            : tr("Wi-Fi: already connecting…"));
        return;
    }
    running_ = true;
    Q_EMIT statusChanged(tr("Starting Wi-Fi for %1…").arg(address_));
    watchdogTimer_->start();
    connectToDevice();
}

void WifiWorker::stop()
{
    running_ = false;
    reconnectTimer_->stop();
    queueTimer_->stop();
    watchdogTimer_->stop();
    pending_.clear();
    resetSocket();
    Q_EMIT statusChanged(tr("Wi-Fi stopped"));
}

void WifiWorker::setAddress(const QString& address)
{
    const QString normalized = address.trimmed();
    if (normalized.isEmpty()) {
        Q_EMIT statusChanged(tr("Invalid address: %1").arg(normalized));
        return;
    }
    if (normalized == address_) {
        Q_EMIT statusChanged(tr("Address: %1").arg(address_));
        return;
    }

    QHostAddress parsed;
    if (!parsed.setAddress(normalized) || parsed.protocol() != QAbstractSocket::IPv4Protocol) {
        Q_EMIT statusChanged(tr("Invalid IP address: %1").arg(normalized));
        return;
    }

    address_ = normalized;
    hostAddress_ = parsed;
    Q_EMIT statusChanged(tr("Address changed: %1").arg(address_));
    pending_.clear();
    if (running_) {
        reconnectTimer_->stop();
        resetSocket();
        scheduleReconnect(0);
    }
}

void WifiWorker::setMinimumInterval(double seconds)
{
    minimumIntervalMs_ = qBound(0, qRound(seconds * 1000.0), 2000);
}

void WifiWorker::setDeduplicationWindow(double seconds)
{
    deduplicationWindowMs_ = qBound(0, qRound(seconds * 1000.0), 5000);
}

void WifiWorker::send(const QByteArray& command)
{
    if (command.isEmpty()) {
        return;
    }

    const qint64 now = nowMs();
    if (command == lastWrittenCommand_
        && now - lastWrittenAtMs_ < deduplicationWindowMs_) {
        return;
    }

    if (protocol::isColorCommand(command)) {
        for (qsizetype index = pending_.size() - 1; index >= 0; --index) {
            if (protocol::isColorCommand(pending_.at(index))) {
                pending_[index] = command;
                processQueue();
                return;
            }
        }
    }

    if (!pending_.isEmpty() && pending_.constLast() == command) {
        return;
    }

    if (pending_.size() >= QueueCapacity) {
        qsizetype removable = -1;
        for (qsizetype index = 0; index < pending_.size(); ++index) {
            if (protocol::isColorCommand(pending_.at(index))) {
                removable = index;
                break;
            }
        }
        if (removable >= 0)
            pending_.removeAt(removable);
        else
            return;
    }
    pending_.enqueue(command);
    processQueue();
}

void WifiWorker::sendUrgent(const QByteArray& command)
{
    if (command.isEmpty()) {
        return;
    }

    pending_.clear();
    queueTimer_->stop();
    if (!running_ || !connected_ || !socket_) {
        return;
    }

    const qint64 written = socket_->write(command);
    if (written != command.size()) {
        Q_EMIT statusChanged(tr("TCP write error, reconnecting…"));
        setConnected(false);
        resetSocket();
        scheduleReconnect(0);
        return;
    }

    lastWrittenCommand_ = command;
    lastWrittenAtMs_ = nowMs();
    lastSuccessfulWriteAtMs_ = lastWrittenAtMs_;
    socket_->flush();
    socket_->waitForBytesWritten(500);
}

void WifiWorker::queryState()
{
    if (connected_) {
        send(protocol::queryStateCommand());
    }
}

void WifiWorker::connectToDevice()
{
    if (!running_ || socket_) {
        return;
    }

    if (hostAddress_.isNull()) {
        QHostAddress parsed;
        if (!parsed.setAddress(address_) || parsed.protocol() != QAbstractSocket::IPv4Protocol) {
            Q_EMIT statusChanged(tr("Invalid IP address: %1").arg(address_));
            scheduleReconnect();
            return;
        }
        hostAddress_ = parsed;
    }

    Q_EMIT statusChanged(tr("Connecting to %1:%2…").arg(hostAddress_.toString()).arg(protocol::Port));

    socket_ = new QTcpSocket(this);
    connect(socket_, &QTcpSocket::connected, this, &WifiWorker::onConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &WifiWorker::onDisconnected);
    connect(socket_, &QTcpSocket::errorOccurred, this, &WifiWorker::onErrorOccurred);

    socket_->connectToHost(hostAddress_, protocol::Port);
}

void WifiWorker::onConnected()
{
    if (!socket_ || sender() != socket_ || !running_) {
        return;
    }
    setConnected(true);
    lastSuccessfulWriteAtMs_ = nowMs();
    Q_EMIT statusChanged(tr("Wi-Fi connected: %1").arg(address_));
    processQueue();
}

void WifiWorker::onDisconnected()
{
    if (!socket_ || sender() != socket_) {
        return;
    }
    setConnected(false);
    pending_.clear();
    resetSocket();
    if (running_) {
        Q_EMIT statusChanged(tr("Wi-Fi disconnected. Retrying in 5 s…"));
        scheduleReconnect();
    }
}

void WifiWorker::onErrorOccurred(QAbstractSocket::SocketError error)
{
    if (!socket_ || sender() != socket_) {
        return;
    }
    const QString message = socket_ ? socket_->errorString() : tr("unknown error");
    Q_EMIT statusChanged(tr("Wi-Fi unavailable (%1), retrying in 5 s…").arg(message));
    setConnected(false);
    pending_.clear();
    resetSocket();
    scheduleReconnect();
    Q_UNUSED(error)
}

void WifiWorker::processQueue()
{
    if (!running_ || !connected_ || !socket_ || pending_.isEmpty()) {
        return;
    }

    const qint64 elapsed = nowMs() - lastWrittenAtMs_;
    if (lastWrittenAtMs_ > 0 && elapsed < minimumIntervalMs_) {
        queueTimer_->start(int(minimumIntervalMs_ - elapsed));
        return;
    }

    const QByteArray command = pending_.dequeue();
    const qint64 written = socket_->write(command);
    if (written != command.size()) {
        pending_.prepend(command);
        Q_EMIT statusChanged(tr("TCP write error, reconnecting…"));
        setConnected(false);
        resetSocket();
        scheduleReconnect(0);
        return;
    }

    lastWrittenCommand_ = command;
    lastWrittenAtMs_ = nowMs();
    lastSuccessfulWriteAtMs_ = lastWrittenAtMs_;

    if (!pending_.isEmpty()) {
        queueTimer_->start(minimumIntervalMs_);
    }
}

void WifiWorker::watchdogTick()
{
    if (!running_) {
        return;
    }
    if (!socket_) {
        scheduleReconnect(0);
        return;
    }
    if (connected_ && !pending_.isEmpty()
        && nowMs() - lastSuccessfulWriteAtMs_ > StalledWriteTimeoutMs) {
        Q_EMIT statusChanged(tr("Wi-Fi: queue not sending, reconnecting…"));
        setConnected(false);
        resetSocket();
        scheduleReconnect(0);
    }
}

void WifiWorker::resetSocket()
{
    if (socket_) {
        disconnect(socket_, nullptr, this, nullptr);
        socket_->abort();
        socket_->deleteLater();
        socket_ = nullptr;
    }
    setConnected(false);
}

void WifiWorker::setConnected(bool connected)
{
    if (connected_ == connected) {
        return;
    }
    connected_ = connected;
    Q_EMIT connectedChanged(connected_);
}

void WifiWorker::scheduleReconnect(int milliseconds)
{
    if (running_ && !reconnectTimer_->isActive()) {
        reconnectTimer_->start(milliseconds);
    }
}

WifiManager::WifiManager(QObject* parent)
    : QObject(parent)
    , worker_(new WifiWorker)
{
    worker_->moveToThread(&thread_);

    connect(this, &WifiManager::startRequested,
            worker_, &WifiWorker::start, Qt::QueuedConnection);
    connect(this, &WifiManager::stopRequested,
            worker_, &WifiWorker::stop, Qt::QueuedConnection);
    connect(this, &WifiManager::addressRequested,
            worker_, &WifiWorker::setAddress, Qt::QueuedConnection);
    connect(this, &WifiManager::minimumIntervalRequested,
            worker_, &WifiWorker::setMinimumInterval, Qt::QueuedConnection);
    connect(this, &WifiManager::deduplicationWindowRequested,
            worker_, &WifiWorker::setDeduplicationWindow, Qt::QueuedConnection);
    connect(this, &WifiManager::sendRequested,
            worker_, &WifiWorker::send, Qt::QueuedConnection);
    connect(this, &WifiManager::queryStateRequested,
            worker_, &WifiWorker::queryState, Qt::QueuedConnection);
    connect(worker_, &WifiWorker::statusChanged,
            this, &WifiManager::statusChanged, Qt::QueuedConnection);
    connect(worker_, &WifiWorker::connectedChanged,
            this, &WifiManager::connectedChanged, Qt::QueuedConnection);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);

    thread_.setObjectName(QStringLiteral("Magic Home TCP"));
    thread_.start();
}

WifiManager::~WifiManager()
{
    if (thread_.isRunning()) {
        QMetaObject::invokeMethod(worker_, &WifiWorker::stop,
                                  Qt::BlockingQueuedConnection);
        thread_.quit();
        thread_.wait();
    }
    worker_ = nullptr;
}

void WifiManager::start()
{
    Q_EMIT startRequested();
}

void WifiManager::stop()
{
    if (thread_.isRunning() && worker_) {
        QMetaObject::invokeMethod(worker_, &WifiWorker::stop,
                                  Qt::BlockingQueuedConnection);
    }
}

void WifiManager::setAddress(const QString& address)
{
    Q_EMIT addressRequested(address);
}

void WifiManager::setMinimumInterval(double seconds)
{
    Q_EMIT minimumIntervalRequested(seconds);
}

void WifiManager::setDeduplicationWindow(double seconds)
{
    Q_EMIT deduplicationWindowRequested(seconds);
}

void WifiManager::send(const QByteArray& command)
{
    Q_EMIT sendRequested(command);
}

void WifiManager::sendUrgent(const QByteArray& command)
{
    if (thread_.isRunning() && worker_) {
        QMetaObject::invokeMethod(worker_, &WifiWorker::sendUrgent, Qt::BlockingQueuedConnection,
                                  command);
    }
}

void WifiManager::sendBlocking(const QByteArray& command)
{
    if (thread_.isRunning() && worker_) {
        QMetaObject::invokeMethod(worker_, &WifiWorker::send, Qt::BlockingQueuedConnection,
                                  command);
    }
}

void WifiManager::queryState()
{
    Q_EMIT queryStateRequested();
}

} // namespace elkbledom