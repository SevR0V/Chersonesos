#include "gamepadworker.h"
#include <QTimer>
#include <QDebug>

const QString GamepadWorker::NO_DEVICE_NAME = "No Device";

GamepadWorker::GamepadWorker(QObject *parent) : QObject(parent)
{
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        qDebug() << "SDL3 Init failed in worker:" << SDL_GetError();
        return;
    }

    currentPrimaryName = NO_DEVICE_NAME;
    currentSecondaryName = NO_DEVICE_NAME;
    updateDeviceList();

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &GamepadWorker::pollDevices);
    timer->start(16);
}

GamepadWorker::~GamepadWorker()
{
    for (SDL_Joystick *joystick : joysticks.values()) {
        if (joystick) SDL_CloseJoystick(joystick);
    }
    SDL_Quit();
}

void GamepadWorker::setPrimaryDevice(const QString &deviceName)
{
    if (deviceName != NO_DEVICE_NAME) {
        deactivateJoystick(primaryJoystick);
        primaryJoystick = joysticks.value(deviceName, nullptr);
        if (primaryJoystick) {
            qDebug() << "Primary Joystick opened:" << deviceName;
            currentPrimaryName = deviceName;
        } else {
            qDebug() << "Failed to set Primary Joystick:" << deviceName << "not found";
            currentPrimaryName = NO_DEVICE_NAME;
        }
    } else {
        deactivateJoystick(primaryJoystick);
        currentPrimaryName = NO_DEVICE_NAME;
    }
}

void GamepadWorker::setSecondaryDevice(const QString &deviceName)
{
    if (deviceName != NO_DEVICE_NAME) {
        deactivateJoystick(secondaryJoystick);
        secondaryJoystick = joysticks.value(deviceName, nullptr);
        if (secondaryJoystick) {
            qDebug() << "Secondary Joystick opened:" << deviceName;
            currentSecondaryName = deviceName;
        } else {
            qDebug() << "Failed to set Secondary Joystick:" << deviceName << "not found";
            currentSecondaryName = NO_DEVICE_NAME;
        }
    } else {
        deactivateJoystick(secondaryJoystick);
        currentSecondaryName = NO_DEVICE_NAME;
    }
}

void GamepadWorker::stop()
{
    running = false;
    timer->stop();
    qDebug() << "GamepadWorker stopped";
}

void GamepadWorker::pollDevices()
{
    if (!running) return;

    SDL_UpdateJoysticks(); // Обновляем состояние всех джойстиков

    int numJoysticks = 0;
    SDL_GetJoysticks(&numJoysticks);
    if (numJoysticks != lastNumJoysticks) {
        updateDeviceList();
    }

    if (primaryJoystick) {
        for (int i = 0; i < SDL_GetNumJoystickButtons(primaryJoystick); ++i) {
            if (SDL_GetJoystickButton(primaryJoystick, i)) {
                emit primaryButtonPressed(i);
            }
        }
        for (int i = 0; i < SDL_GetNumJoystickAxes(primaryJoystick); ++i) {
            Sint16 axisValue = SDL_GetJoystickAxis(primaryJoystick, i);
            if (abs(axisValue) > 1000) {
                emit primaryAxisMoved(i, axisValue);
            }
        }
    }

    if (secondaryJoystick) {
        for (int i = 0; i < SDL_GetNumJoystickButtons(secondaryJoystick); ++i) {
            if (SDL_GetJoystickButton(secondaryJoystick, i)) {
                emit secondaryButtonPressed(i);
            }
        }
        for (int i = 0; i < SDL_GetNumJoystickAxes(secondaryJoystick); ++i) {
            Sint16 axisValue = SDL_GetJoystickAxis(secondaryJoystick, i);
            if (abs(axisValue) > 1000) {
                emit secondaryAxisMoved(i, axisValue);
            }
        }
    }
}

void GamepadWorker::updateDeviceList()
{
    int numJoysticks = 0;
    SDL_JoystickID *joystickIds = SDL_GetJoysticks(&numJoysticks);
    if (numJoysticks == lastNumJoysticks && !deviceListChanged) {
        SDL_free(joystickIds);
        return;
    }

    lastNumJoysticks = numJoysticks;
    deviceListChanged = true;

    QMap<QString, SDL_Joystick*> newJoysticks;
    for (const QString &name : joysticks.keys()) {
        if (name != currentPrimaryName && name != currentSecondaryName) {
            SDL_Joystick *joystick = joysticks[name];
            if (joystick) SDL_CloseJoystick(joystick);
        } else {
            newJoysticks[name] = joysticks[name];
        }
    }
    joysticks = newJoysticks;

    QStringList deviceNames;
    deviceNames << NO_DEVICE_NAME;

    if (joystickIds && numJoysticks > 0) {
        for (int i = 0; i < numJoysticks; ++i) {
            SDL_Joystick *tempJoystick = SDL_OpenJoystick(joystickIds[i]);
            if (tempJoystick) {
                const char *name = SDL_GetJoystickName(tempJoystick);
                QString deviceName = name ? name : QString("Joystick %1").arg(joystickIds[i]);
                if (!joysticks.contains(deviceName)) {
                    joysticks[deviceName] = tempJoystick;
                } else {
                    SDL_CloseJoystick(tempJoystick);
                }
                deviceNames << deviceName;
            }
        }
        SDL_free(joystickIds);
    }

    emit deviceListUpdated(deviceNames);
}

void GamepadWorker::deactivateJoystick(SDL_Joystick *&joystick)
{
    if (joystick) {
        QString nameToRemove;
        for (const QString &name : joysticks.keys()) {
            if (joysticks[name] == joystick && name != currentPrimaryName && name != currentSecondaryName) {
                nameToRemove = name;
                break;
            }
        }
        if (!nameToRemove.isEmpty()) {
            joysticks.remove(nameToRemove);
            SDL_CloseJoystick(joystick);
        }
        joystick = nullptr;
        qDebug() << "Joystick deactivated";
    }
}
