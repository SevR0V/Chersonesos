#include "udphandler.h"
#include <QDebug>

UdpHandler::UdpHandler(QObject *parent)
    : QObject(parent),
    socket(new QUdpSocket(this)),
    remoteAddress("127.0.0.1"),
    remotePort(12345)
{
    socket->bind(QHostAddress::AnyIPv4, 0); // Привязываемся к любому локальному порту
    connect(socket, &QUdpSocket::readyRead, this, &UdpHandler::onReadyRead);
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
