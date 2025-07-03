#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QByteArray>
#include <QHostAddress>
#include "gamepadworker.h"
#include "profilemanager.h"
#include "udptelemetryparser.h"

class UdpHandler : public QObject {
    Q_OBJECT

public:
    explicit UdpHandler(ProfileManager *profileManager,
                        UdpTelemetryParser *telemetryParser,
                        GamepadWorker *gamepadWorker,
                        QObject *parent = nullptr);
    void sendDatagram(const QByteArray &data);
    void setRemoteEndpoint(const QHostAddress &address, quint16 port);

signals:
    void datagramReceived(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);

private slots:
    void onReadyRead();
    void onJoystickDataChange(DualJoystickState joysticsState);

private:
    QUdpSocket *socket;
    QHostAddress remoteAddress;
    quint16 remotePort;

    ProfileManager *profileManager;
    UdpTelemetryParser *telemetryParser;
    GamepadWorker *gamepadWorker;
};
