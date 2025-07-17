#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QByteArray>
#include <QHostAddress>
#include <QJsonArray>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QMultiMap>
#include <QTimer>
#include "gamepadworker.h"
#include "profilemanager.h"
#include "udptelemetryparser.h"
#include "SettingsManager.h"

class UdpHandler : public QObject {
    Q_OBJECT

public:
    explicit UdpHandler(ProfileManager *profileManager,
                        UdpTelemetryParser *telemetryParser,
                        GamepadWorker *gamepadWorker,
                        QObject *parent = nullptr);
    void sendDatagram(const QByteArray &data);
    bool getMasterState();
    bool getLigthsState();
    float getPowerLimit();
    void getThrust(const float forward,
                   const float strafe,
                   const float vertical,
                   const float roll,
                   const float pitch,
                   const float yaw);
    bool connectToROV(const QHostAddress &address, quint16 port);
    bool getOnlineStatus;


signals:
    void datagramReceived(const QByteArray &data,
                          const QHostAddress &sender,
                          quint16 senderPort);
    void recordingStartStop();
    void takeFrame();
    void updateMaster();
    void onlineStateChanged(const bool &onlineState);

public slots:
    void settingsChanged();

private slots:
    void onReadyRead();
    void onJoystickDataChange(const DualJoystickState joysticsState);

private:
    QUdpSocket *socket;
    QHostAddress remoteAddress;
    quint16 remotePort;

    ProfileManager *profileManager;
    UdpTelemetryParser *telemetryParser;
    GamepadWorker *gamepadWorker;

    TelemetryPacket telemetryData;
    QMultiMap<QString, QString> inputToMachine;
    QMultiMap<QString, QString> machineToInput;

    float getControlValue(QString action,
                          const QMultiMap<QString, QString>& controlMap,
                          const DualJoystickState& joysticsState,
                          const QJsonObject& controlProfile,
                          const QString inc = "inc",
                          const QString dec = "dec");

    void setRemoteEndpoint(const QHostAddress &address, quint16 port);

    void loadMappingsFromJson(const QString& filePath);
    void onlineTimerTick();

    const float DEADZONE = 5;

    QByteArray packControlData();
    QTimer *onlineTimer;
    qint64 lastOnlineTime;

    SettingsManager *settingsManager = nullptr;

    bool onlineFlag;
    float cForwardThrust;
    float cSideThrust;
    float cVerticalThrust;
    float cYawThrust;
    float cRollThrust;
    float cPitchThrust;
    float cCameraRotate;
    float cManipulatorRotate;
    float cManipulatorGrip;
    float cPowerLimit;
    float RollKP;
    float RollKI;
    float RollKD;
    float PitchKP;
    float PitchKI;
    float PitchKD;
    float YawKP;
    float YawKI;
    float YawKD;
    float DepthKP;
    float DepthKI;
    float DepthKD;
    bool cLights;
    bool cPosReset;
    bool cMASTER;
    bool cRecording;
    bool cRollStab ;
    bool cPitchStab ;
    bool cYawStab ;
    bool cDepthStab ;
    bool cUpdatePID ;
    bool cResetIMU ;

    bool lightsValueChangeFlag;
    bool masterValueChangeFlag;
    bool recordingValueChangeFlag;
    bool takeFrameValueChangeFlag;
    bool prevRecordingButtonState;
};
