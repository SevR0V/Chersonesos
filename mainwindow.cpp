#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "controlwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // connect(ui->powerSlider, &QSlider::valueChanged, ui->powerValuelcdNumber, &QLCDNumber::set)
}

MainWindow::~MainWindow()
{
    delete ui;
}

void controlsButtonPressed()
{

}
