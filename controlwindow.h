#ifndef CONTROLWINDOW_H
#define CONTROLWINDOW_H

#include <QWidget>
#include <QThread>
#include "gamepadworker.h"
#include <SDL3/SDL.h>

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
    explicit ControlWindow(QWidget *parent = nullptr);
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

public slots:
    void stopProgressCountdown();

private:
    Ui::ControlWindow *ui;
    // гуишные штучки
    void replaceLineEdits(QWidget* widget);
    void replaceLineEditsInWidget(QWidget *widget, QWidget *mainWidget);
    QTimer *progressTimer;
    // геймпаддные штучки
    QString currentPrimaryName;
    QString currentSecondaryName;

    QThread *workerThread;
    GamepadWorker *worker;
};

#endif // CONTROLWINDOW_H
