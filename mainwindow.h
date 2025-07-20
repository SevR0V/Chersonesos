#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QOpenGLWidget>
#include <QHBoxLayout>
#include <QMutex>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QKeyEvent>
#include "ui_mainwindow.h"
#include "camera.h"
#include "controlwindow.h"
#include "profilemanager.h"
#include "settingsdialog.h"
#include "udphandler.h"
#include "gamepadworker.h"
#include "udptelemetryparser.h"
#include <winspool.h>
#include <QResizeEvent>
#include "SettingsManager.h"
#include "overlaywidget.h"

class OverlayWidget;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void startCameraSignal();
    void stopAllCamerasSignal();
    void startRecordingSignal(const QString& cameraName, int recordInterval, int storedVideoFilesLimit);
    void stopRecordingSignal(const QString& cameraName);
    void startStreamingSignal(const QString& cameraName, int port);
    void stopStreamingSignal(const QString& cameraName);
    void stereoShotSignal();
    void masterChanged(const bool& masterState);
    void stabUpdated(const bool& stabAllState,
                     const bool& stabRollState,
                     const bool& stabPitchState,
                     const bool& stabYawState,
                     const bool& stabDepthState);

private slots:
    void processFrame(CameraFrameInfo* camera);
    void handleCameraError(const QString& component, const QString& message);
    void handleCameraSuccess(const QString& component, const QString& message);
    void afterReconnect(Camera* camera);
    void on_takeStereoframeButton_clicked();
    void onDatagramReceived(const QByteArray &data, const QHostAddress &sender, quint16 port);
    void onJoystickUpdate(const DualJoystickState &state);
    void onlineStateChanged(const bool &onlineState);
    void settingsChanged();
    void updatePID();
    void resetAngle();
    void activeProfileChanged();
    void updateOverlayData();
    void updateMasterFromControl(const bool &masterState);
    void telemetryReceived(const TelemetryPacket &packet);
    void setStabState();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    Ui::MainWindow *ui;
    Camera* m_camera;
    QMap<QString, QOpenGLWidget*> m_displayWidgets;
    QHBoxLayout* m_cameraLayout;
    ControlWindow *controlsWindow;
    SettingsDialog *settingsDialog;
    QLabel* m_label;
    void masterSwitch();
    void onResize();
    void startRecord();
    bool isStereoRecording;
    void showHideLeftPanel();
    bool isPanelHidden;
    bool isRecording;
    bool masterState;
    int powerLimit;
    float camAngle;

    bool stabEnabled;
    bool stabRollEnabled;
    bool stabPitchEnabled;
    bool stabYawEnabled;
    bool stabDepthEnabled;

    UdpTelemetryParser *telemetryParser;
    TelemetryPacket telemetryPacket;
    ProfileManager *profileManager;

    QThread *udpThread;
    UdpHandler *udpHandler;
    QThread *workerThread;
    GamepadWorker *worker;
    OverlayWidget* m_overlay;
};

#endif // MAINWINDOW_H
