#ifndef UDPTELEMETRYPARSER_H
#define UDPTELEMETRYPARSER_H

#include <QObject>
#include <QByteArray>
#include <QDataStream>

struct TelemetryPacket {
    quint64 flags;       // Статусные флаги
    float roll;          // Крен
    float pitch;         // Тангаж
    float yaw;           // Курс
    float depth;         // Глубина
    float batVoltage;    // Напряжение батареи
    float batCharge;     // Заряд батареи
    float cameraAngle;   // Угол поворота камеры
    float rollSP;        // Задание крена
    float pitchSP;       // Задание тангажа
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
