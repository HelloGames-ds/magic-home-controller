#pragma once

#include <QAbstractSocket>
#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QQueue>
#include <QThread>
#include <QTimer>

class QTcpSocket;

namespace elkbledom {

class WifiWorker final : public QObject
{
    Q_OBJECT

public:
    explicit WifiWorker(QObject* parent = nullptr);
    ~WifiWorker() override;

    QString address() const;
    bool isConnected() const noexcept;

public Q_SLOTS:
    void start();
    void stop();
    void setAddress(const QString& address);
    void setMinimumInterval(double seconds);
    void setDeduplicationWindow(double seconds);
    void send(const QByteArray& command);
    void sendUrgent(const QByteArray& command);
    void queryState();

Q_SIGNALS:
    void statusChanged(const QString& status);
    void connectedChanged(bool connected);
    void deviceReachableChanged(bool reachable);

private Q_SLOTS:
    void connectToDevice();
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onReadyRead();
    void processQueue();
    void watchdogTick();
    void pingTick();

private:
    void resetSocket();
    void setConnected(bool connected);
    void setDeviceReachable(bool reachable);
    void scheduleReconnect(int milliseconds = -1);
    int nextReconnectDelay();

    QString address_;
    QHostAddress hostAddress_;
    QTcpSocket* socket_ = nullptr;
    QQueue<QByteArray> pending_;

    QTimer* reconnectTimer_ = nullptr;
    QTimer* queueTimer_ = nullptr;
    QTimer* watchdogTimer_ = nullptr;
    QTimer* pingTimer_ = nullptr;

    QByteArray lastWrittenCommand_;
    qint64 lastWrittenAtMs_ = 0;
    qint64 lastSuccessfulWriteAtMs_ = 0;
    qint64 pingSentAtMs_ = 0;
    qint64 connectedAtMs_ = 0;
    int reconnectDelayMs_ = 2000;
    int missedPings_ = 0;
    int minimumIntervalMs_ = 0;
    int deduplicationWindowMs_ = 500;
    bool running_ = false;
    bool connected_ = false;
    bool deviceReachable_ = false;
};

class WifiManager final : public QObject
{
    Q_OBJECT

public:
    explicit WifiManager(QObject* parent = nullptr);
    ~WifiManager() override;

    void start();
    void stop();
    void setAddress(const QString& address);
    void setMinimumInterval(double seconds);
    void setDeduplicationWindow(double seconds);
    void send(const QByteArray& command);
    void sendUrgent(const QByteArray& command);
    void sendBlocking(const QByteArray& command);
    void queryState();

Q_SIGNALS:
    void startRequested();
    void stopRequested();
    void addressRequested(const QString& address);
    void minimumIntervalRequested(double seconds);
    void deduplicationWindowRequested(double seconds);
    void sendRequested(const QByteArray& command);
    void queryStateRequested();
    void statusChanged(const QString& status);
    void connectedChanged(bool connected);
    void deviceReachableChanged(bool reachable);

private:
    QThread thread_;
    WifiWorker* worker_ = nullptr;
};

} // namespace elkbledom