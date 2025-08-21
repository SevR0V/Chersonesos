#include "UdpTelemetryParser.h"
#include <QDebug>

UdpTelemetryParser::UdpTelemetryParser(QObject *parent)
    : QObject(parent)
{
}

bool UdpTelemetryParser::parse(const QByteArray &data, TelemetryPacket &packet)
{
    if (data.size() != 44)
        return false;

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    stream >> packet.flags;
    stream >> packet.roll;
    stream >> packet.pitch;
    stream >> packet.yaw;
    stream >> packet.depth;
    stream >> packet.batVoltage;
    stream >> packet.batCharge;
    stream >> packet.cameraAngle;
    stream >> packet.rollSP;
    stream >> packet.pitchSP;
    emit telemetryReceived(packet);
    return true;
}
