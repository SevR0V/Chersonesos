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
    // connect(ui->powerSlider, &QSlider::valueChanged, ui->powerSpinBox, &QSpinBox::setValue);
    connect(ui->controlsButton, &QPushButton::pressed, [this]() {controlsWindow->show();});
    ui->videoWidget->setStyleSheet("background-color: lightblue");
    // connect(this, & , this, &MainWindow::onResize);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    // Получаем старый и новый размер
    // QSize oldSize = event->oldSize();
    // QSize newSize = event->size();
    QSize videoSize = ui->videoWidget->frameSize();
    int videoWidth = videoSize.width();
    int videoHeight = videoSize.height();
    float videoAR = videoWidth/videoHeight;

    // Пример обработки
    // qDebug() << "Старый размер:" << oldSize;
    // qDebug() << "Новый размер:" << newSize;

    // Можно изменять размеры/положение дочерних виджетов
    // ui->someWidget->setGeometry(10, 10, newSize.width() - 20, newSize.height() - 20);

    // Обязательно вызываем реализацию базового класса
    QMainWindow::resizeEvent(event);
}
// void MainWindow::controlsButtonPressed()
// {
//     controlsWindow->show();
// }
