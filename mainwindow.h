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
    ControlWindow *controlsWindow;
    void controlsButtonPressed();
    void onResize();
    void startRecord();
    bool isRecording;
    Camera* m_camera;
    QMap<QString, QOpenGLWidget*> m_displayWidgets;
    QHBoxLayout* m_cameraLayout;
};

#endif // MAINWINDOW_H
