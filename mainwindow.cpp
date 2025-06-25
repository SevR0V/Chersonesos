#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <winspool.h>
#include <QResizeEvent>

void setMasterButtonState(QPushButton *button, const bool masterState, const bool isPanelHidden);
void setRecordButtonState(QPushButton *button, const bool isRecording, const bool isPanelHidden);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QStringList names = {"LCamera", "RCamera"};
    m_camera = new Camera(names, this);
    m_cameraLayout = new QHBoxLayout();
    ui->videoWidget->setLayout(m_cameraLayout);
    ui->videoWidget->setMinimumSize(800, 600);
    ui->videoWidget->setStyleSheet("background-color: transparent;");

    m_label = new QLabel();
    m_label->setScaledContents(true);
    m_label->setAlignment(Qt::AlignCenter);
    m_cameraLayout->addWidget(m_label);

    const QList<CameraFrameInfo*>& cameras = m_camera->getCameras();
    for (CameraFrameInfo* cam : cameras) {
        if (cam->name == "LCamera") {
            cam->labelWinId = 2;
        } else {
            cam->labelWinId = 1;
        }
        qDebug() << "Установлен labelWinId для камеры" << cam->name << ":" << cam->labelWinId;
    }

    connect(m_camera, &Camera::greatSuccess, this, &MainWindow::handleCameraSuccess);
    connect(m_camera, &Camera::frameReady, this, &MainWindow::processFrame);
    connect(m_camera, &Camera::errorOccurred, this, &MainWindow::handleCameraError);
    connect(m_camera, &Camera::reconnectDone, this, &MainWindow::afterReconnect);
    connect(m_camera, &Camera::finished, this, &QMainWindow::close);

    m_camera->start();
    qDebug() << "Камера запущена.";

    QTimer::singleShot(5000, this, [this]() {
        m_camera->startStreaming("LCamera", 8080);
        m_camera->startStreaming("RCamera", 8081);
    });

    controlsWindow = new ControlWindow;
    settingsDialog = new SettingsDialog;

    connect(ui->controlsButton, &QPushButton::pressed, [this]() { controlsWindow->show(); });
    connect(ui->settingsButton, &QPushButton::pressed, [this]() { settingsDialog->show(); });
    connect(ui->startRecordButton, &QPushButton::pressed, this, &MainWindow::startRecord);
    connect(ui->hideShowButton, &QPushButton::pressed, this, &MainWindow::showHideLeftPanel);
    connect(ui->masterButton, &QPushButton::pressed, this, &MainWindow::masterSwitch);

    ui->powerGroupBox->setStyleSheet("QGroupBox { border: 0px; }");

    QIcon locked(":/Resources/Icons/lock_closed.ico");
    ui->masterButton->setIcon(locked);
    ui->masterButton->setIconSize(QSize(28, 28));

    QIcon leftArrow(":/Resources/Icons/left_arrow.ico");
    ui->hideShowButton->setIcon(leftArrow);
    ui->hideShowButton->setIconSize(QSize(28, 28));

    QIcon icon(":/Resources/Icons/circle_black.ico");
    ui->startRecordButton->setIcon(icon);
    ui->startRecordButton->setIconSize(QSize(14, 14));

    isPanelHidden = false;
    isRecording = false;
    masterState = false;
    isStereoRecording = ui->recordStereoCheckBox->isChecked();
}

MainWindow::~MainWindow()
{
    delete m_camera;
    delete ui;
}

void MainWindow::processFrame(CameraFrameInfo* camera)
{
    QMutexLocker lock(camera->mutex);

    if (camera->name == "LCamera") {
        m_label->setPixmap(QPixmap::fromImage(camera->img));
    }
}

void MainWindow::showHideLeftPanel()
{
    isPanelHidden = !isPanelHidden;
    if(isPanelHidden){
        QIcon rightArrow(":/Resources/Icons/right_arrow.ico");
        ui->hideShowButton->setIcon(rightArrow);

        ui->masterButton->setText("");
        ui->takeStereoframeButton->setText("");
        ui->startRecordButton->setText("");
        ui->controlsButton->setText("");
        ui->settingsButton->setText("");

        ui->onlineLable->hide();
        ui->recordStereoCheckBox->hide();
        ui->powerGroupBox->hide();
        ui->rovStateLabel->hide();
        ui->powerLabel->hide();
        ui->recordLabel->hide();
        ui->settingsLabel->hide();
        ui->powerBotLine->hide();
    } else {
        QIcon leftArrow(":/Resources/Icons/left_arrow.ico");
        ui->hideShowButton->setIcon(leftArrow);

        setMasterButtonState(ui->masterButton, masterState, isPanelHidden);
        ui->takeStereoframeButton->setText("Сделать стереокадр      ");
        setRecordButtonState(ui->startRecordButton,isRecording, isPanelHidden);
        ui->controlsButton->setText("Управление");
        ui->settingsButton->setText("Настройки");
        ui->onlineLable->setText("Не в сети");

        ui->onlineLable->show();
        ui->recordStereoCheckBox->show();
        ui->powerGroupBox->show();
        ui->powerBotLine->show();
        ui->rovStateLabel->show();
        ui->powerLabel->show();
        ui->recordLabel->show();
        ui->settingsLabel->show();
    }
}

void MainWindow::handleCameraError(const QString& component, const QString& message)
{

}

void MainWindow::handleCameraSuccess(const QString& component, const QString& message)
{

}

void MainWindow::afterReconnect(Camera* camera)
{
    if (isRecording) {
        m_camera->startRecording("LCamera", 120, 0); // Имя камеры, интервал записи в секундах, ограничение по количеству файлов
        if (isStereoRecording) {
            m_camera->startRecording("RCamera", 120, 0);
        }
    }
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

void MainWindow::masterSwitch()
{
    masterState = !masterState;
    setMasterButtonState(ui->masterButton, masterState, isPanelHidden);
}

void setMasterButtonState(QPushButton *button, const bool masterState, const bool isPanelHidden)
{
    if(masterState)
    {
        if(! isPanelHidden)
            button->setText("Заблокировать ТНПА");
        QIcon unlocked(":/Resources/Icons/lock_open.ico");
        button->setIcon(unlocked);
    } else {
        if(! isPanelHidden)
            button->setText("Разблокировать ТНПА");
        QIcon locked(":/Resources/Icons/lock_closed.ico");
        button->setIcon(locked);
    }
}

void MainWindow::startRecord()
{
    ui->recordStereoCheckBox->setDisabled(!isRecording);
    isRecording = !isRecording;
    isStereoRecording = ui->recordStereoCheckBox->isChecked();

    setRecordButtonState(ui->startRecordButton, isRecording, isPanelHidden);

    if (isRecording) {
        m_camera->startRecording("LCamera", 120, 0);
        if (isStereoRecording) {
            m_camera->startRecording("RCamera", 120, 0);
        }
    } else {
        m_camera->stopRecording("LCamera");
        if (isStereoRecording) {
            m_camera->stopRecording("RCamera");
        }
    }
}

void setRecordButtonState(QPushButton *button, const bool isRecording, const bool isPanelHidden)
{
    if(isRecording)
    {
        if(! isPanelHidden)
            button->setText("Остановить запись");
        QIcon icon(":/Resources/Icons/circle_red.ico");
        button->setIcon(icon);
    } else {
        if(! isPanelHidden)
            button->setText("Записать видео");
        QIcon icon(":/Resources/Icons/circle_black.ico");
        button->setIcon(icon);
    }
}

void MainWindow::controlsButtonPressed() {
    controlsWindow->show();
}

void MainWindow::settingsButtonPressed() {
    settingsDialog->show();
}

void MainWindow::onResize() {
    if (m_cameraLayout) {
        m_cameraLayout->update();
    }
}
