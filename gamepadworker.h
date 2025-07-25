#ifndef GAMEPADWORKER_H
#define GAMEPADWORKER_H

#include <QObject>
#include <SDL3/SDL.h>
#include <QMap>

struct JoystickState {
    QString deviceName;
    QVector<Sint16> axes;
    QVector<bool> buttons;
    QVector<QString> hats;
};

struct DualJoystickState {
    JoystickState primary;
    JoystickState secondary;
};

Q_DECLARE_METATYPE(JoystickState)
Q_DECLARE_METATYPE(DualJoystickState)

class GamepadWorker : public QObject {
    Q_OBJECT

public:
    explicit GamepadWorker(QObject *parent = nullptr);
    ~GamepadWorker();

    void setPrimaryDevice(const QString &deviceName);
    void setSecondaryDevice(const QString &deviceName);
    bool hasDeviceListChanged() const { return deviceListChanged; }
    void pollDevices();
    void updateDeviceList();
    void resetDeviceListChanged() { deviceListChanged = false; }

public slots:
    void stop();

signals:
    void primaryButtonPressed(int button);
    void primaryAxisMoved(int axis, Sint16 value);
    void secondaryButtonPressed(int button);
    void secondaryAxisMoved(int axis, Sint16 value);
    void deviceListUpdated(const QStringList &deviceNames);
    void primaryHatPressed(int hat, const QString &direction);
    void secondaryHatPressed(int hat, const QString &direction);
    void joysticksUpdated(const DualJoystickState &state);

private:
    void deactivateJoystick(SDL_Joystick *&joystick);

    SDL_Joystick *primaryJoystick = nullptr;
    SDL_Joystick *secondaryJoystick = nullptr;
    QString currentPrimaryName;
    QString currentSecondaryName;
    int lastNumJoysticks = -1;
    bool deviceListChanged = false;
    QTimer *timer;
    bool running = true;
    QMap<QString, SDL_Joystick*> joysticks;
    std::vector<Sint16> primaryAxisValues;
    std::vector<Sint16> secondaryAxisValues;

    static const QString NO_DEVICE_NAME;
};

#endif // GAMEPADWORKER_H
