#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "controlwindow.h"
#include <QResizeEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    controlsWindow = new ControlWindow;
    // *controlsWindow->profileManager = *MainWindow::profileManager;
    connect(ui->controlsButton, &QPushButton::pressed, [this]() {controlsWindow->show();});
    connect(ui->startRecordButton, &QPushButton::pressed, this, &MainWindow::startRecord);
    ui->videoWidget->setStyleSheet("background-color: lightblue");
    isRecording = false;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{

}

void MainWindow::startRecord()
{
    ui->recordStereoCheckBox->setDisabled(!isRecording);
    if(isRecording)
    {
        ui->startRecordButton->setText("Записать видео");
    } else {
        ui->startRecordButton->setText("Остановить запись");
    }
    isRecording = !isRecording;
    bool isStereoRecording = ui->recordStereoCheckBox->isChecked();
}
