#pragma once

#include <QObject>
#include <QSet>
#include <QString>

class QTimer;
class QUdpSocket;

namespace elkbledom {

class DeviceScanner final : public QObject
{
    Q_OBJECT

public:
    explicit DeviceScanner(QObject* parent = nullptr);
    ~DeviceScanner() override;

    bool isActive() const;

public Q_SLOTS:
    void start();
    void stop();

Q_SIGNALS:
    void scanStarted();
    void deviceFound(const QString& address, const QString& name);
    void finished();
    void error(const QString& message);

private Q_SLOTS:
    void onDatagram();
    void onTimeout();

private:
    void broadcastDiscovery();

    QUdpSocket* socket_ = nullptr;
    QTimer* timeoutTimer_ = nullptr;
    QSet<QString> seenAddresses_;
};

} // namespace elkbledom