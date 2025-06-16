#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QByteArray>
#include <QHostAddress>

class UdpHandler : public QObject {
    Q_OBJECT

public:
    explicit UdpHandler(QObject *parent = nullptr);
    void sendDatagram(const QByteArray &data);
    void setRemoteEndpoint(const QHostAddress &address, quint16 port);

signals:
    void datagramReceived(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);

private slots:
    void onReadyRead();

private:
    QUdpSocket *socket;
    QHostAddress remoteAddress;
    quint16 remotePort;
};
