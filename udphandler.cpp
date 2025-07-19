#include "udphandler.h"
#include <QCoreApplication>
#include <QDir>
#include <QVariant>
#include <cstdlib>
#include <QDebug>
#include <QNetworkInterface>
#include <cmath>

/*TODO
 Связь все управлений и телеметрии с основным окном!!!*/

UdpHandler::UdpHandler(ProfileManager *profileManager, UdpTelemetryParser *telemetryParser, GamepadWorker *gamepadWorker, QObject *parent)
    : QObject(parent),
    socket(new QUdpSocket(this)),
    remoteAddress("192.168.88.246"),
    remotePort(1337),
    profileManager(profileManager),
    telemetryParser(telemetryParser),
    gamepadWorker(gamepadWorker)
{
    bool ok = socket->bind(QHostAddress::AnyIPv4, 4444, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!ok) {
        qWarning() << "[UdpHandler] Bind failed:" << socket->errorString();
    } else {
        qDebug() << "[UdpHandler] Listening";
    }
    connect(socket, &QUdpSocket::readyRead, this, &UdpHandler::onReadyRead);
    connect(gamepadWorker, &GamepadWorker::joysticksUpdated, this, &UdpHandler::onJoystickDataChange);

    cForwardThrust = 0;
    cSideThrust = 0;
    cVerticalThrust = 0;
    cYawThrust = 0;
    cRollThrust = 0;
    cPitchThrust = 0;
    cCameraRotate = 0;
    cManipulatorRotate = 0;
    cManipulatorGrip = 0;
    cPowerLimit = 0; // 0..1
    iPowerLimit = 0;
    RollKP = 0;
    RollKI = 0;
    RollKD = 0;
    PitchKP = 0;
    PitchKI = 0;
    PitchKD = 0;
    YawKP = 0;
    YawKI = 0;
    YawKD = 0;
    DepthKP = 0;
    DepthKI = 0;
    DepthKD = 0;
    cLights = false;
    cPosReset = false;
    cMASTER = false;
    cRecording = false;
    cRollStab = false;
    cPitchStab = false;
    cYawStab = false;
    cDepthStab = false;
    cUpdatePID = false;
    cResetIMU = false;

    lightsValueChangeFlag = false;
    masterValueChangeFlag = false;
    recordingValueChangeFlag = false;
    takeFrameValueChangeFlag = false;
    prevRecordingButtonState = false;

    QString baseDir = QCoreApplication::applicationDirPath();
    loadMappingsFromJson(baseDir + QDir::separator() +
                         "Configs" + QDir::separator() + "Control mapping.cfg");

    //Таймер для проверки подключения к аппарату
    onlineFlag = false;
    lastOnlineTime = 0;
    onlineTimer = new QTimer(this);
    connect(onlineTimer, &QTimer::timeout, this, &UdpHandler::onlineTimerTick);
    onlineTimer->start(1000);

    incremental = new QTimer(this);
    connect(incremental, &QTimer::timeout, this, &UdpHandler::incrementValues);
    incremental->start(50);

    qDebug() << "Local IPs:";
    for (const QHostAddress &addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && addr != QHostAddress::LocalHost)
            qDebug() << addr.toString();
    }
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
        // qDebug() << "[UdpHandler] Sent" << sent << "bytes to" << remoteAddress.toString() << ":" << remotePort;
    }
}

void UdpHandler::onReadyRead() {

    while (socket->hasPendingDatagrams()) {
        qint64 size = socket->pendingDatagramSize(); //
        if (size <= 0) {
            qWarning() << "[UdpHandler] Invalid datagram size:" << size;
            socket->readDatagram(nullptr, 0); // очистим очередь
            continue;
        }

        QByteArray buffer;
        buffer.resize(size);

        QHostAddress sender;
        quint16 senderPort;

        qint64 bytesRead = socket->readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);
        if (bytesRead == -1) {
            qWarning() << "[UdpHandler] Failed to read datagram:" << socket->errorString();
        } /*else {
            qDebug() << "[UdpHandler] Received" << bytesRead << "bytes from" << sender.toString() << ":" << senderPort;
            qDebug() << "[UdpHandler] Data:" << buffer.toHex();
        }*/
        // qDebug() << "Data size:" << buffer.size();
        // qDebug() << "Data hex:" << buffer.toHex();
        if(buffer.size() == 44){
            telemetryParser->parse(buffer, telemetryData);
            lastOnlineTime = QDateTime::currentSecsSinceEpoch();
        }
    }
}

void UdpHandler::onlineTimerTick(){
    qint64 currentSecs = QDateTime::currentSecsSinceEpoch();
    if((currentSecs - lastOnlineTime) > 2){
        if(onlineFlag){
            onlineFlag = false;
            emit onlineStateChanged(onlineFlag);
            return;
        } else {
            if(connectToROV(remoteAddress, remotePort)){

            }
        }
    } else if(!onlineFlag){
        onlineFlag = true;
        emit onlineStateChanged(onlineFlag);
    }
    // qDebug() << "Online: " << onlineFlag;
    // qDebug() << currentSecs - lastOnlineTime;
}

void UdpHandler::loadMappingsFromJson(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file:" << filePath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format";
        return;
    }

    QJsonObject root = doc.object();

    for (auto it = root.begin(); it != root.end(); ++it) {
        QString machineAction = it.key();
        QJsonArray inputs = it.value().toArray();

        for (const QJsonValue& inputVal : inputs) {
            QString input = inputVal.toString();
            inputToMachine.insert(input, machineAction);
            machineToInput.insert(machineAction, input);
        }
    }
}

QByteArray UdpHandler::packControlData()
{
    //Flags
    uint64_t controlFlags = 0;
    controlFlags |= quint64(cMASTER)        << 0;
    controlFlags |= quint64(cLights)        << 1;
    controlFlags |= quint64(cRollStab)      << 2;
    controlFlags |= quint64(cPitchStab)     << 3;
    controlFlags |= quint64(cYawStab)       << 4;
    controlFlags |= quint64(cDepthStab)     << 5;
    controlFlags |= quint64(cPosReset)      << 6;
    controlFlags |= quint64(cResetIMU)      << 7;
    controlFlags |= quint64(cUpdatePID)     << 8;
    if(cUpdatePID) cUpdatePID = false;

    //Data
    QList<float> floats = {
        cForwardThrust,
        cSideThrust,
        cVerticalThrust,
        cYawThrust,
        cRollThrust,
        cPitchThrust,
        cPowerLimit,
        cCameraRotate,
        cManipulatorGrip,
        cManipulatorRotate,
        RollKP, RollKI, RollKD,
        PitchKP, PitchKI, PitchKD,
        YawKP, YawKI, YawKD,
        DepthKP, DepthKI, DepthKD
    };

    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    //Add flaggs to stream
    stream << controlFlags;

    //Add data to stream
    for (float val : floats) {
        stream << val;
    }
    // qDebug() << "Sending packet:" << buffer.toHex();
    return buffer;
}

void UdpHandler::onJoystickDataChange(const DualJoystickState joysticsState){
    if(!onlineFlag) return;
    QJsonObject controlProfile = profileManager->getProfile();

    //Manipulator rotate

    cManipulatorRotate = getControlValue("manipulator_rotate", machineToInput, joysticsState, controlProfile, "Right", "Left");

    //Manipulator grip
    cManipulatorGrip = getControlValue("manipulator_grip", machineToInput, joysticsState, controlProfile, "Open", "Close");

    //Power limit incremental
    iPowerLimit = getControlValue("power_limit", machineToInput, joysticsState, controlProfile);

    //Camera rotate
    cCameraRotate = getControlValue("camera_rotate", machineToInput, joysticsState, controlProfile, "Up", "Down");

    //Reset stabilization setpoints
    float positionResetButtonState = getControlValue("position_reset", machineToInput, joysticsState, controlProfile, "PosReset");
    cPosReset = positionResetButtonState? true : false;

    //Lights on off
    float lightsButtonState = getControlValue("lights", machineToInput, joysticsState, controlProfile, "On");
    if(lightsButtonState){
        if (!lightsValueChangeFlag){
            cLights = !cLights;
            lightsValueChangeFlag = true;
        }
    } else {
        lightsValueChangeFlag = false;
    }

    //Record video start stop
    float recordingButtonState = getControlValue("recording", machineToInput, joysticsState, controlProfile, "Start");
    if(recordingButtonState){
        if (!recordingValueChangeFlag){

            // cRecording = !cRecording;
            emit recordingStartStop();
            recordingValueChangeFlag = true;
        }
    } else {
        recordingValueChangeFlag = false;
    }

    //Take frame
    float takeFrameButtonState = getControlValue("take_frame", machineToInput, joysticsState, controlProfile, "Stereoframe");
    if(takeFrameButtonState){
        if (!takeFrameValueChangeFlag){
            emit takeFrame();
            qDebug() << "Took a shot";
            takeFrameValueChangeFlag = true;
        }
    } else {
        takeFrameValueChangeFlag = false;
    }

    //MASTER on off
    float masterButtonState = getControlValue("master_switch", machineToInput, joysticsState, controlProfile, "Master");
    if(masterButtonState){
        if (!masterValueChangeFlag){
            cMASTER = !cMASTER;
            emit updateMaster(cMASTER);
            masterValueChangeFlag = true;
        }
    } else {
        masterValueChangeFlag = false;
    }

    //Forward
    cForwardThrust = getControlValue("forward_thrust", machineToInput, joysticsState, controlProfile, "Forward", "Backward");

    //Strafe
    cSideThrust = getControlValue("side_thrust", machineToInput, joysticsState, controlProfile, "Right", "Left");

    //Vertical
    cVerticalThrust = getControlValue("vertical_thrust", machineToInput, joysticsState, controlProfile, "Up", "Down");

    //Yaw
    cYawThrust = getControlValue("rotate_yaw", machineToInput, joysticsState, controlProfile, "Right", "Left");

    //Roll
    cRollThrust = getControlValue("rotate_roll", machineToInput, joysticsState, controlProfile);

    //Pitch
    cPitchThrust = getControlValue("rotate_pitch", machineToInput, joysticsState, controlProfile);

    if(onlineFlag)
        sendDatagram(packControlData());

    // qDebug() << "Forward thrust: " << cForwardThrust;
    // qDebug() << "Side thrust: " << cSideThrust;
    // qDebug() << "Vertical thrust: " << cVerticalThrust;
    // qDebug() << "Yaw thrust: " << cYawThrust;
    // qDebug() << "Roll thrust: " << cRollThrust;
    // qDebug() << "Pitch thrust: " << cPitchThrust;
    // qDebug() << "Camera rotate: " << cCameraRotate;
    // qDebug() << "Manipulator rotate: " << cManipulatorRotate;
    // qDebug() << "Manipulator grip: " << cManipulatorGrip;
    // qDebug() << "Power limit increment:" << iPowerLimit;
    // qDebug() << "Lights: " << cLights;
    // qDebug() << "Position reset: " << cPosReset;
    // qDebug() << "MASTER Switch: " << cMASTER;
    // qDebug() << "Video recording: " << cRecording;
}

std::pair<QString, bool> findInputByInputName(const QJsonObject& rootObj, const QString& targetInputName) {
    QJsonArray mappings = rootObj["inputs"].toArray();

    for (const QJsonValue& val : mappings) {
        QJsonObject obj = val.toObject();
        if (obj["inputName"].toString() == targetInputName) {
            QString input = obj["input"].toString();
            bool inversion = obj["inversion"].toBool(false); // если поля нет — false
            // qDebug() << input << " is inverted: " << inversion;
            return std::make_pair(input, inversion);
        }
    }

    // если не найдено — вернуть пустую строку и false
    return std::make_pair("", false);
}

QVariant getInputValue(const QString &joyInput,
                       const JoystickState &joyState){
    QStringList parts = joyInput.split(' ', Qt::SkipEmptyParts);
    QString inputType = parts[0];
    int inputId = parts[1].toInt();
    if(inputType.contains(("hat"), Qt::CaseInsensitive)){
        QString hatAction = parts[0].split('_', Qt::SkipEmptyParts)[1];
        // qDebug() << "Raw joy value:" << parts[0] <<" : "<< inputId << " : " << hatAction << " value: " << joyState.hats[inputId].contains(hatAction, Qt::CaseInsensitive);
        return joyState.hats[inputId].contains(hatAction, Qt::CaseInsensitive);
    }else if(inputType.contains(("button"), Qt::CaseInsensitive)){
        // qDebug() << "Raw joy value:" << parts[0] <<" : "<< inputId << " value: " << joyState.buttons[inputId];
        return joyState.buttons[inputId];
    }else if(inputType.contains(("axis"), Qt::CaseInsensitive)){
        // qDebug() << "Raw joy value:" << parts[0] <<" : "<< inputId << " value: " << joyState.axes[inputId];
        return joyState.axes[inputId];
    }
    return 0;
}

float mapSInt16ToFloat(Sint16 x, Sint16 in_min, Sint16 in_max, float out_min, float out_max)
{
    if (in_max == in_min)
        return out_min; // защита от деления на 0

    return float(x - in_min) * (out_max - out_min) / float(in_max - in_min) + out_min;
}

float UdpHandler::getControlValue(QString action,
                      const QMultiMap<QString, QString>& controlMap,
                      const DualJoystickState& joysticsState,
                      const QJsonObject& controlProfile,
                      const QString inc,
                      const QString dec) {
    JoystickState primaryJoyState = joysticsState.primary;
    JoystickState secondaryJoyState = joysticsState.secondary;

    QList<QString> controls = controlMap.values(action);
    QString primaryJoystick = controlProfile["devices"]["primary"].toString();
    QString secondaryJoystick = controlProfile["devices"]["secondary"].toString();
    bool isPrimaryOnline = !((primaryJoystick.contains("[offline]", Qt::CaseInsensitive)) || (primaryJoystick == "No Device"));
    bool isSecondaryOnline = !((secondaryJoystick.contains("[offline]", Qt::CaseInsensitive)) || (secondaryJoystick == "No Device"));
    if(primaryJoyState.deviceName != primaryJoystick) isPrimaryOnline = false;
    if(secondaryJoyState.deviceName != secondaryJoystick) isSecondaryOnline = false;
    if(! (isPrimaryOnline || isSecondaryOnline)){
        return 0;
    }
    bool primaryIncBut = false;
    bool primaryDecBut = false;
    bool secondaryIncBut = false;
    bool secondaryDecBut = false;
    Sint16 primaryAxis = 0;
    Sint16 secondaryAxis = 0;
    //axis > buttons, primary > secondary

    for(const QString& str : controls) {
        if(str.contains("secondary", Qt::CaseInsensitive) &&
           str.contains("but", Qt::CaseInsensitive) &&
           str.contains(inc, Qt::CaseInsensitive) &&
           isSecondaryOnline) {
            auto input = findInputByInputName(controlProfile, str);
            QString joyInput = input.first;
            if (!joyInput.isEmpty()){
                QVariant inputValue = getInputValue(joyInput, secondaryJoyState);
                if (inputValue.typeId() == QMetaType::Bool) {
                    secondaryIncBut = inputValue.toBool();
                }else{
                    secondaryIncBut = false;
                }
            }
        }

        if(str.contains("secondary", Qt::CaseInsensitive) &&
            str.contains("but", Qt::CaseInsensitive) &&
            str.contains(dec, Qt::CaseInsensitive) &&
            isSecondaryOnline) {
            auto input = findInputByInputName(controlProfile, str);
            QString joyInput = input.first;
            if (!joyInput.isEmpty()){
                QVariant inputValue = getInputValue(joyInput, secondaryJoyState);
                if (inputValue.typeId() == QMetaType::Bool) {
                    secondaryDecBut = inputValue.toBool();
                }else{
                    secondaryDecBut = false;
                }
            }
        }

        if(str.contains("primary", Qt::CaseInsensitive) &&
            str.contains("but", Qt::CaseInsensitive) &&
            str.contains(inc, Qt::CaseInsensitive) &&
            isPrimaryOnline) {
            auto input = findInputByInputName(controlProfile, str);
            QString joyInput = input.first;
            if (!joyInput.isEmpty()){
                QVariant inputValue = getInputValue(joyInput, secondaryJoyState);
                if (inputValue.typeId() == QMetaType::Bool) {
                    primaryIncBut = inputValue.toBool();
                }else{
                    primaryIncBut = false;
                }
            }
        }

        if(str.contains("primary", Qt::CaseInsensitive) &&
            str.contains("but", Qt::CaseInsensitive) &&
            str.contains(dec, Qt::CaseInsensitive) &&
            isPrimaryOnline) {
            auto input = findInputByInputName(controlProfile, str);
            QString joyInput = input.first;
            if (!joyInput.isEmpty()){
                QVariant inputValue = getInputValue(joyInput, secondaryJoyState);
                if (inputValue.typeId() == QMetaType::Bool) {
                    primaryDecBut = inputValue.toBool();
                }else{
                    primaryDecBut = false;
                }
            }
        }

        if(str.contains("primary", Qt::CaseInsensitive) &&
            !str.contains("but", Qt::CaseInsensitive) &&
            isPrimaryOnline) {
            auto input = findInputByInputName(controlProfile, str);
            QString joyInput = input.first;
            bool inversion = input.second;
            if (!joyInput.isEmpty()){
                QVariant inputValue = getInputValue(joyInput, secondaryJoyState);
                if (inputValue.typeId() == QMetaType::Bool) {
                    primaryAxis = 0;
                }else{
                    if(!inversion){
                        primaryAxis = mapSInt16ToFloat(inputValue.value<Sint16>(), -32768, 32767, -100.0f, 100.0f);
                    }else{
                        primaryAxis = -mapSInt16ToFloat(inputValue.value<Sint16>(), -32768, 32767, -100.0f, 100.0f);
                    }
                }
            }
        }

        if(str.contains("secondary", Qt::CaseInsensitive) &&
            !str.contains("but", Qt::CaseInsensitive) &&
            isSecondaryOnline) {
            auto input = findInputByInputName(controlProfile, str);
            QString joyInput = input.first;
            bool inversion = input.second;
            if (!joyInput.isEmpty()){
                QVariant inputValue = getInputValue(joyInput, secondaryJoyState);
                if (inputValue.typeId() == QMetaType::Bool) {
                    secondaryAxis = 0;
                }else{
                    if(!inversion){
                        secondaryAxis = mapSInt16ToFloat(inputValue.value<Sint16>(), -32768, 32767, -100.0f, 100.0f);
                    }else{
                        secondaryAxis = -mapSInt16ToFloat(inputValue.value<Sint16>(), -32768, 32767, -100.0f, 100.0f);
                    }
                }
            }
        }
    }

    float value = 0;
    if (std::abs(primaryAxis) < DEADZONE){
        primaryAxis = 0;
    }
    if (std::abs(secondaryAxis) < DEADZONE){
        secondaryAxis = 0;
    }

    value = (secondaryIncBut - secondaryDecBut) * 100;
    value = value == 0 ? ((primaryIncBut - primaryDecBut) * 100) : value;
    value = secondaryAxis == 0 ? value : secondaryAxis;
    value = primaryAxis == 0 ? value : primaryAxis;
    value = value/100.0f;
    return value;
}

bool UdpHandler::connectToROV(const QHostAddress &address, quint16 port){
    // setRemoteEndpoint(address, port);
    QByteArray packet;
    // socket->connectToHost(address, port);
    packet.append(char(0xAA));
    packet.append(char(0xFF));
    sendDatagram(packet);
    return true;
}

void UdpHandler::settingsChanged(){
    SettingsManager &settingsManager = SettingsManager::instance();
    QString ip = settingsManager.getString("ip");
    quint16 port = settingsManager.getInt("portEdit");
    remoteAddress.setAddress(ip);
    setRemoteEndpoint(remoteAddress, port);
}

void UdpHandler::masterChangedGui(const bool &masterState)
{
    cMASTER = masterState;
}

float constrainf(const float value, const float lower_limit, const float upper_limit){
    if(value>upper_limit) return upper_limit;
    if(value<lower_limit) return lower_limit;
    return value;
}

void UdpHandler::incrementValues(){
    cPowerLimit = cPowerLimit + float(iPowerLimit/50.0f);
    cPowerLimit = constrainf(cPowerLimit, 0.0f, 1.0f);
    emit updatePowerLimit(std::round(cPowerLimit * 100));
}


void UdpHandler::updatePowerLimitFromGui(const int &powerLimit){
    cPowerLimit = powerLimit / 100.0f;
}

void UdpHandler::updatePID(){
    SettingsManager &settingsManager = SettingsManager::instance();
    RollKP = settingsManager.getDouble("RollkP");
    RollKI = settingsManager.getDouble("RollkI");
    RollKD = settingsManager.getDouble("RollkD");
    PitchKP = settingsManager.getDouble("PitchkP");
    PitchKI = settingsManager.getDouble("PitchkI");
    PitchKD = settingsManager.getDouble("PitchkD");
    YawKP = settingsManager.getDouble("YawkP");
    YawKI = settingsManager.getDouble("YawkI");
    YawKD = settingsManager.getDouble("YawkD");
    DepthKP = settingsManager.getDouble("DepthkP");
    DepthKI = settingsManager.getDouble("DepthkI");
    DepthKD = settingsManager.getDouble("DepthkD");
    cUpdatePID = true;
}
