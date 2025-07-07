#include "udphandler.h"
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QJsonObject>

UdpHandler::UdpHandler(ProfileManager *profileManager, UdpTelemetryParser *telemetryParser, GamepadWorker *gamepadWorker, QObject *parent)
    : QObject(parent),
    socket(new QUdpSocket(this)),
    remoteAddress("127.0.0.1"),
    remotePort(12345),
    profileManager(profileManager),
    telemetryParser(telemetryParser),
    gamepadWorker(gamepadWorker)
{
    socket->bind(QHostAddress::AnyIPv4, 0); // Привязываемся к любому локальному порту
    connect(socket, &QUdpSocket::readyRead, this, &UdpHandler::onReadyRead);
    connect(gamepadWorker, &GamepadWorker::joysticksUpdated, this, &UdpHandler::onJoystickDataChange);
}

void UdpHandler::setRemoteEndpoint(const QHostAddress &address, quint16 port) {
    remoteAddress = address;
    remotePort = port;
    qDebug() << "[UdpHandler] Remote endpoint updated to" << remoteAddress.toString() << ":" << remotePort;
}

void UdpHandler::sendDatagram(const QByteArray &data) {
    if (remoteAddress.isNull() || remotePort == 0) {
        qWarning() << "[UdpHandler] Remote address or port not set!";
        return;
    }

    qint64 sent = socket->writeDatagram(data, remoteAddress, remotePort);
    if (sent == -1) {
        qWarning() << "[UdpHandler] Failed to send datagram:" << socket->errorString();
    } else {
        qDebug() << "[UdpHandler] Sent" << sent << "bytes to" << remoteAddress.toString() << ":" << remotePort;
    }
}

void UdpHandler::onReadyRead() {
    while (socket->hasPendingDatagrams()) {
        QByteArray buffer;
        buffer.resize(socket->pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;
        socket->readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);

        emit datagramReceived(buffer, sender, senderPort);
    }
}

void UdpHandler::onJoystickDataChange(DualJoystickState joysticsState){
    JoystickState primaryJoyState = joysticsState.primary;
    JoystickState secondaryJoyState = joysticsState.secondary;
    QJsonObject controlProfile = profileManager->getProfile();


}
