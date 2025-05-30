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
#include "controlwindow.h"
#include "settingsdialog.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void processFrame(CameraFrameInfo* camera);
    void handleCameraError(const QString& component, const QString& message);
    void afterReconnect(Camera* camera);

    void on_takeStereoframeButton_clicked();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    Ui::MainWindow *ui;
    Camera* m_camera;
    QMap<QString, QOpenGLWidget*> m_displayWidgets;
    QHBoxLayout* m_cameraLayout;
    ControlWindow *controlsWindow;
    SettingsDialog *settingsDialog;
    void controlsButtonPressed();
    void settingsButtonPressed();
    void masterSwitch();
    void onResize();
    void startRecord();
    void showHideLeftPanel();
    bool isPanelHidden;
    bool isRecording;
    bool masterState;
};

#endif // MAINWINDOW_H
