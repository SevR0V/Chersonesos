#ifndef UDPTELEMETRYPARSER_H
#define UDPTELEMETRYPARSER_H

#include <QObject>
#include <QByteArray>
#include <QDataStream>

struct TelemetryPacket {
    quint64 flags = 0;// Статусные флаги
    float roll = 0;// Крен
    float pitch = 0;// Тангаж
    float yaw = 0;// Курс
    float depth = 0;// Глубина
    float batVoltage = 0;// Напряжение батареи
    float batCharge = 0;// Заряд батареи
    float cameraAngle = 0;// Угол поворота камеры
    float rollSP = 0;// Задание крена
    float pitchSP = 0;// Задание тангажа
};

class UdpTelemetryParser : public QObject {
    Q_OBJECT

public:
    explicit UdpTelemetryParser(QObject *parent = nullptr);

    bool parse(const QByteArray &data, TelemetryPacket &packet);

signals:
    void telemetryReceived(const TelemetryPacket &packet);
};

#endif // UDPTELEMETRYPARSER_H
