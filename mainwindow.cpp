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
    ui->videoWidget->setStyleSheet("background-color: lightblue");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{

}

