#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "controlwindow.h"
#include "profilemanager.h"
#include "controlwindow.h"
#include "settingsdialog.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    ProfileManager *profileManager;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::MainWindow *ui;
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
