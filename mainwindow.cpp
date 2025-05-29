#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <winspool.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    controlsWindow = new ControlWindow;
    // *controlsWindow->profileManager = *MainWindow::profileManager;
    connect(ui->controlsButton, &QPushButton::pressed, [this]() {controlsWindow->show();});
    connect(ui->startRecordButton, &QPushButton::pressed, this, &MainWindow::startRecord);
    ui->videoWidget->setStyleSheet("background-color: lightblue");
    isRecording = false;
    // Инициализация камеры
    m_camera = new Camera(this);
    m_camera->setCameraNames({"LCamera", "RCamera"});
    m_camera->initializeCameras();

    // Макет для камеры внутри videoWidget
    m_cameraLayout = new QHBoxLayout();
    ui->videoWidget->setLayout(m_cameraLayout);
    ui->videoWidget->setMinimumSize(800, 600);
    ui->videoWidget->setStyleSheet("background-color: lightblue;");

    // Инициализация labelWinId для обеих камер
    const QList<CameraFrameInfo*>& cameras = m_camera->getCameras();
    for (CameraFrameInfo* cam : cameras) {
        if (cam->name == "LCamera") {
            cam->labelWinId = ui->videoWidget->winId(); // Для LCamera — вывод в videoWidget
        } else {
            cam->labelWinId = 1; // Для RCamera — не отображаем
        }
        qDebug() << "Установлен labelWinId для камеры" << cam->name << ":" << cam->labelWinId;
    }

    // Подключение сигналов
    connect(m_camera, &Camera::frameReady, this, &MainWindow::processFrame);
    connect(m_camera, &Camera::errorOccurred, this, &MainWindow::handleCameraError);
    connect(m_camera, &Camera::reconnectDone, this, &MainWindow::afterReconnect);
    connect(m_camera, &Camera::finished, this, &QMainWindow::close);

    // Запуск камеры
    m_camera->start();
    qDebug() << "Камера запущена.";

    QTimer::singleShot(5000, this, [this]() { // Задержка для записи
        m_camera->startRecording("LCamera", 120, 100);
        m_camera->startRecording("RCamera", 120, 100);
    });

    QTimer::singleShot(5000, this, [this]() { // Задержка для стрима
        m_camera->startStreaming("LCamera", 8080);
        m_camera->startStreaming("RCamera", 8081);
    });
}

MainWindow::~MainWindow()
{
    delete m_camera;
    delete ui;
}

void MainWindow::processFrame(CameraFrameInfo* camera)
{
    QMutexLocker lock(camera->mutex);

    camera->frame.hWnd = reinterpret_cast<void*>(camera->labelWinId);
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
void MainWindow::handleCameraError(const QString& component, const QString& message)
{
    //QMessageBox::critical(this, "Camera Error", QString("Ошибка в %1: %2").arg(component).arg(message));
}

void MainWindow::afterReconnect(Camera* camera)
{
    m_camera->startRecording("LCamera", 120, 100);
    m_camera->startRecording("RCamera", 120, 100);
    m_camera->startStreaming("LCamera", 8080);
    m_camera->startStreaming("RCamera", 8081);
    qDebug() << "Переподключение выполнено";
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        m_camera->stopAll();
        close();
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::on_takeStereoframeButton_clicked()
{
    m_camera->stereoShot();
}
