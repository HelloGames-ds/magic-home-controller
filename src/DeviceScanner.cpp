#include "DeviceScanner.h"

#include "protocol.h"

#include <QByteArray>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>

namespace elkbledom {
namespace {

constexpr int DiscoverTimeoutMs = 2000;

QList<QHostAddress> broadcastAddresses()
{
    QSet<QHostAddress> result;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& interface : interfaces) {
        if (!(interface.flags() & QNetworkInterface::IsUp)
            || (interface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const QList<QNetworkAddressEntry> entries = interface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            const QHostAddress broadcast = entry.broadcast();
            if (!broadcast.isNull()) {
                result.insert(broadcast);
            }
        }
    }
    result.insert(QHostAddress(QStringLiteral("255.255.255.255")));
    return result.values();
}

} // namespace

DeviceScanner::DeviceScanner(QObject* parent)
    : QObject(parent)
    , socket_(new QUdpSocket(this))
    , timeoutTimer_(new QTimer(this))
{
    timeoutTimer_->setSingleShot(true);
    timeoutTimer_->setInterval(DiscoverTimeoutMs);
    connect(timeoutTimer_, &QTimer::timeout, this, &DeviceScanner::onTimeout);
    connect(socket_, &QUdpSocket::readyRead, this, &DeviceScanner::onDatagram);
}

DeviceScanner::~DeviceScanner()
{
    stop();
}

bool DeviceScanner::isActive() const
{
    return socket_ && socket_->isValid();
}

void DeviceScanner::start()
{
    if (socket_->isValid()) {
        stop();
    }

    seenAddresses_.clear();
    const bool bound = socket_->bind(
        QHostAddress(QStringLiteral("0.0.0.0")), protocol::DiscoveryPort,
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!bound) {
        Q_EMIT error(socket_->errorString());
        return;
    }

    Q_EMIT scanStarted();
    broadcastDiscovery();
    timeoutTimer_->start();
}

void DeviceScanner::stop()
{
    timeoutTimer_->stop();
    socket_->abort();
}

void DeviceScanner::broadcastDiscovery()
{
    const QByteArray payload(protocol::DiscoveryPayload);
    const QList<QHostAddress> addresses = broadcastAddresses();
    for (const QHostAddress& address : addresses) {
        socket_->writeDatagram(payload, address, protocol::DiscoveryPort);
    }
}

void DeviceScanner::onDatagram()
{
    while (socket_->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(socket_->pendingDatagramSize()));
        socket_->readDatagram(datagram.data(), datagram.size());

        const QStringList parts = QString::fromUtf8(datagram).trimmed().split(QLatin1Char(','));
        if (parts.size() < 3) {
            continue;
        }

        const QString address = parts.at(0).trimmed();
        const QString model = parts.at(2).trimmed();
        if (address.isEmpty() || seenAddresses_.contains(address)) {
            continue;
        }

        seenAddresses_.insert(address);
        Q_EMIT deviceFound(address, model.isEmpty() ? QStringLiteral("Magic Home") : model);
    }
}

void DeviceScanner::onTimeout()
{
    if (!socket_->isValid()) {
        return;
    }
    socket_->abort();
    Q_EMIT finished();
}

} // namespace elkbledom