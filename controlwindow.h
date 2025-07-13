#ifndef CONTROLWINDOW_H
#define CONTROLWINDOW_H

#include <QWidget>
#include <QThread>
#include "gamepadworker.h"
#include <SDL3/SDL.h>
#include "profilemanager.h"

namespace Ui {
class ControlWindow;
}

class ControlWindow : public QWidget
{
    Q_OBJECT

signals:
    void primaryButtonPressed(int button);
    void primaryAxisMoved(int axis, Sint16 value);
    void secondaryButtonPressed(int button);
    void secondaryAxisMoved(int axis, Sint16 value);

public:
    explicit ControlWindow(GamepadWorker *worker, ProfileManager *profileManager, QWidget *parent = nullptr);
    ~ControlWindow();
    void controlsButtonPressed();
    bool isJoyListenerFinished;

private slots:
    // гуишные штучки
    void onLineEditLeftClicked(QString name);
    void onLineEditRightClicked(QString name);
    void startProgressCountdown();
    void updateProgress();
    // геймпаддные штучки
    void updateDeviceList(const QStringList &deviceNames);
    void onPrimaryDeviceChanged(int index);
    void onSecondaryDeviceChanged(int index);
    void checkForDeviceChanges();
    void onPrimaryButtonPressed(int button);
    void onPrimaryAxisMoved(int axis, Sint16 value);
    void onSecondaryButtonPressed(int button);
    void onSecondaryAxisMoved(int axis, Sint16 value);
    void onPrimaryHatPressed(int hat, QString direction);
    void onSecondaryHatPressed(int hat, QString direction);
    void onLoadProfileBtnClick();

public slots:
    void stopProgressCountdown();

private:
    Ui::ControlWindow *ui;
    // гуишные штучки
    void replaceLineEdits(QWidget* widget);
    void replaceLineEditsInWidget(QWidget *widget, QWidget *mainWidget);
    void onInversionCBvalueChange(bool checked);
    QTimer *progressTimer;
    void onSaveButtonPressed();
    void updateProfileOnGUI();
    // геймпаддные штучки
    QString currentPrimaryDeviceName;
    QString currentSecondaryDeviceName;
    QString activeInputName;

    ProfileManager *profileManager;

    GamepadWorker *_worker;
    void profileActionDetected(QString inputType, int inputIdx);
};

#endif // CONTROLWINDOW_H
