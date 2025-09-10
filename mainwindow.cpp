#include "mainwindow.h"
#include "ui_mainwindow.h"

void setMasterButtonState(QPushButton *button, const bool masterState, const bool isPanelHidden);
void setRecordButtonState(QPushButton *button, const bool isRecording, const bool isPanelHidden);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
    udpThread(new QThread(this)),
    workerThread(new QThread(this)),
    worker(new GamepadWorker()),
    m_overlay(nullptr)
{
    ui->setupUi(this);
    // Инициализируем менеджер настроек один раз при запуске
    if(!SettingsManager::instance().initialize()) {
        qCritical() << "Failed to load settings!";
    }

    isPanelHidden = false;
    isRecording = false;
    masterState = false;
    powerLimit = 0;
    stabEnabled = false;
    stabRollEnabled = false;
    stabPitchEnabled = false;
    stabYawEnabled = false;
    stabDepthEnabled = false;
    camAngle = 0;
    lightsState = false;

    lastRecordToggleTime = QDateTime();

    worker->moveToThread(workerThread);
    workerThread->start();
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::started, worker, &GamepadWorker::pollDevices);
    connect(workerThread, &QThread::finished, worker, &GamepadWorker::deleteLater);
    connect(worker, &GamepadWorker::joysticksUpdated, this, &MainWindow::onJoystickUpdate);

    connect(ui->enableStabCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::setStabState);

    m_overlayFrameInfo = new OverlayFrameInfo();
    // Создание и настройка потока Camera
    QThread* cameraThread = new QThread(this);
    QStringList names = {"LCamera", "RCamera"};
    m_camera = new Camera(names, m_overlayFrameInfo);
    m_camera->moveToThread(cameraThread);

    // Подключение сигналов MainWindow к слотам Camera
    connect(this, &MainWindow::startCameraSignal, m_camera, &Camera::startCamera, Qt::QueuedConnection);
    connect(this, &MainWindow::stopAllCamerasSignal, m_camera, &Camera::stopAllCameras, Qt::QueuedConnection);
    connect(this, &MainWindow::startRecordingSignal, m_camera, &Camera::startRecordingSlot, Qt::QueuedConnection);
    connect(this, &MainWindow::stopRecordingSignal, m_camera, &Camera::stopRecordingSlot, Qt::QueuedConnection);
    connect(this, &MainWindow::startStreamingSignal, m_camera, &Camera::startStreamingSlot, Qt::QueuedConnection);
    connect(this, &MainWindow::stopStreamingSignal, m_camera, &Camera::stopStreamingSlot, Qt::QueuedConnection);
    connect(this, &MainWindow::stereoShotSignal, m_camera, &Camera::stereoShotSlot, Qt::QueuedConnection);

    connect(m_camera, &Camera::frameReady, this, [this](CameraFrameInfo* camera) {
        if (camera->name == "LCamera") {
            m_overlay->pushOverlayToQueueSlot();
        }
    }, Qt::QueuedConnection);

    // Подключение сигналов Camera к слотам MainWindow
    connect(m_camera, &Camera::greatSuccess, this, &MainWindow::handleCameraSuccess);
    connect(m_camera, &Camera::frameReady, this, &MainWindow::processFrame);
    connect(m_camera, &Camera::errorOccurred, this, &MainWindow::handleCameraError);
    connect(m_camera, &Camera::reconnectDone, this, &MainWindow::afterReconnect);
    connect(m_camera, &Camera::finished, this, &QMainWindow::close);

    cameraThread->start();

    // Запуск камеры через сигнал
    emit startCameraSignal();

    m_cameraLayout = new QHBoxLayout();
    ui->videoWidget->setLayout(m_cameraLayout);
    ui->videoWidget->setMinimumSize(800, 600);
    ui->videoWidget->setStyleSheet("background-color: transparent;");

    m_label = new QLabel();
    m_label->setScaledContents(true);
    m_label->setAlignment(Qt::AlignCenter);
    m_cameraLayout->addWidget(m_label);

    m_overlay = new OverlayWidget(m_label, m_overlayFrameInfo);
    m_overlay->setGeometry(0, 0, m_label->width(), m_label->height());
    m_overlay->show();

    const QList<CameraFrameInfo*>& cameras = m_camera->getCameras();
    for (CameraFrameInfo* cam : cameras) {
        if (cam->name == "LCamera") {
            cam->labelWinId = 2;
        } else {
            cam->labelWinId = 1;
        }
        qDebug() << "Установлен labelWinId для камеры" << cam->name << ":" << cam->labelWinId;
    }

    QTimer::singleShot(5000, this, [this]() {
        emit startStreamingSignal("LCamera", 8080);
        emit startStreamingSignal("RCamera", 8081);
    });


    profileManager = new ProfileManager();
    controlsWindow = new ControlWindow(worker, profileManager);
    settingsDialog = new SettingsDialog;

    connect(ui->controlsButton, &QPushButton::pressed, this, [this]() { controlsWindow->show(); });
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


    // isStereoRecording = ui->recordStereoCheckBox->isChecked();

    UdpTelemetryParser *telemetryParser = new UdpTelemetryParser();

    udpHandler = new UdpHandler(profileManager, telemetryParser, worker);

    udpHandler->settingsChanged();

    connect(settingsDialog, &SettingsDialog::settingsChanged, udpHandler, &UdpHandler::settingsChanged);
    connect(settingsDialog, &SettingsDialog::settingsChangedPID, this, &MainWindow::updatePID);
    connect(settingsDialog, &SettingsDialog::settingsChangedAngle, this, &MainWindow::resetAngle);

    udpHandler->moveToThread(udpThread);
    udpThread->start();
    connect(udpThread, &QThread::finished, udpHandler, &QObject::deleteLater);
    connect(udpHandler, &UdpHandler::datagramReceived,
            this, &MainWindow::onDatagramReceived);
    connect(udpHandler, &UdpHandler::onlineStateChanged,
            this, &MainWindow::onlineStateChanged);
    connect(udpHandler, &UdpHandler::recordingStartStop, this, &MainWindow::startRecord);
    connect(udpHandler, &UdpHandler::takeFrame, this, &MainWindow::on_takeStereoframeButton_clicked);

    SettingsManager &settingsManager = SettingsManager::instance();

    controlsWindow->loadProfile(settingsManager.getLastActiveProfile());

    connect(controlsWindow->profileManager, &ProfileManager::profileNameChange, this, &MainWindow::activeProfileChanged);

    connect(udpHandler, &UdpHandler::updateMaster, this, &MainWindow::updateMasterFromControl);
    connect(udpHandler, &UdpHandler::updatePowerLimit, ui->powerSlider, &QSlider::setValue);
    connect(ui->powerSlider, &QSlider::valueChanged, udpHandler, &UdpHandler::updatePowerLimitFromGui);
    connect(ui->powerSlider, &QSlider::valueChanged, [this](const int &value){
        this->powerLimit = value;
    });
    connect(this, &MainWindow::masterChanged, udpHandler, &UdpHandler::masterChangedGui);
    connect(settingsDialog, &SettingsDialog::settingsChangedPID, udpHandler, &UdpHandler::updatePID);
    connect(m_overlay, &OverlayWidget::requestOverlayDataUpdate, this, &MainWindow::updateOverlayData);
    connect(telemetryParser, &UdpTelemetryParser::telemetryReceived, this, &MainWindow::telemetryReceived);

    connect(ui->enableDepthStabCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::setStabState);
    connect(ui->enableRollStabCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::setStabState);
    connect(ui->enablePitchStabCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::setStabState);
    connect(ui->enableYawStabCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::setStabState);
    connect(this, &MainWindow::stabUpdated, udpHandler, &UdpHandler::stabStateChanged);
    connect(udpHandler, &UdpHandler::lightStateChanged,
            this, &MainWindow::updateLightState,
            Qt::QueuedConnection);
}

MainWindow::~MainWindow() {
    if (m_camera) {
        emit stopAllCamerasSignal();  // Убедитесь, что всё остановлено
        QThread* cameraThread = m_camera->thread();
        if (cameraThread && cameraThread->isRunning()) {
            cameraThread->quit();
            if (!cameraThread->wait(10000)) {  // Увеличьте таймаут
                qWarning() << "Camera thread не завершился! Не удаляем m_camera.";
                return;  // Не delete, чтобы избежать краша (утечка, но лучше краша)
            }
        }
        delete m_camera;
        m_camera = nullptr;
    }

    delete m_overlay;
    delete ui;
    workerThread->quit();
    workerThread->wait();
    udpThread->quit();
    udpThread->wait();
}

void MainWindow::onlineStateChanged(const bool &onlineState){
    if(onlineState){
        ui->onlineLable->setText("В сети");
        ui->onlineLable->setStyleSheet("QLabel { color : green; }");
    }else{
        ui->onlineLable->setText("Не в сети");
        ui->onlineLable->setStyleSheet("QLabel { color : red; }");
    }
}

void MainWindow::processFrame(CameraFrameInfo* camera)
{
    QImage img;
    {
        QMutexLocker lock(camera->mutex);
        if (camera->sharedImg) {
            img = *camera->sharedImg;
        }
    }
    if (!img.isNull() && camera->name == "LCamera") {
        m_label->setPixmap(QPixmap::fromImage(img));
        m_overlay->setGeometry(0, 0, m_label->width(), m_label->height());
    } else {
        //qDebug() << "Пустой кадр или неверная камера для отображения:" << camera->name;
    }
}

void MainWindow::showHideLeftPanel()
{
    isPanelHidden = !isPanelHidden;
    if (isPanelHidden) {
        QIcon rightArrow(":/Resources/Icons/right_arrow.ico");
        ui->hideShowButton->setIcon(rightArrow);

        ui->masterButton->setText("");
        ui->takeStereoframeButton->setText("");
        ui->startRecordButton->setText("");
        ui->controlsButton->setText("");
        ui->settingsButton->setText("");
        ui->openStereoProcessingButton->setText("");

        ui->onlineLable->hide();
        // ui->recordStereoCheckBox->hide();
        ui->powerGroupBox->hide();
        ui->rovStateLabel->hide();
        ui->powerLabel->hide();
        ui->recordLabel->hide();
        ui->settingsLabel->hide();
        ui->powerBotLine->hide();
        ui->enableStabCheckBox->hide();
        ui->enableDepthStabCheckBox->hide();
        ui->enableYawStabCheckBox->hide();
        ui->enableRollStabCheckBox->hide();
        ui->enablePitchStabCheckBox->hide();
        ui->stabLabel->hide();
    } else {
        QIcon leftArrow(":/Resources/Icons/left_arrow.ico");
        ui->hideShowButton->setIcon(leftArrow);

        setMasterButtonState(ui->masterButton, masterState, isPanelHidden);
        ui->takeStereoframeButton->setText("Сделать стереокадр      ");
        setRecordButtonState(ui->startRecordButton, isRecording, isPanelHidden);
        ui->controlsButton->setText("Управление");
        ui->settingsButton->setText("Настройки");
        ui->onlineLable->setText("Не в сети");
        ui->openStereoProcessingButton->setText("Обработка \nстереокадров");

        ui->onlineLable->show();
        // ui->recordStereoCheckBox->show();
        ui->powerGroupBox->show();
        ui->powerBotLine->show();
        ui->rovStateLabel->show();
        ui->powerLabel->show();
        ui->recordLabel->show();
        ui->settingsLabel->show();
        ui->enableStabCheckBox->show();
        ui->enableDepthStabCheckBox->show();
        ui->enableYawStabCheckBox->show();
        ui->enableRollStabCheckBox->show();
        ui->enablePitchStabCheckBox->show();
        ui->stabLabel->show();
    }
}

void MainWindow::handleCameraError(const QString& component, const QString& message)
{
}

void MainWindow::handleCameraSuccess(const QString& component, const QString& message)
{
}

void MainWindow::afterReconnect() {
    if (isRecording) {
        emit startRecordingSignal("LCamera", 120, 0, NoOverlay);  // Добавлен RecordMode
        if (isStereoRecording) {
            emit startRecordingSignal("RCamera", 120, 0, NoOverlay);  // Добавлен RecordMode
        }
    }
    emit startStreamingSignal("LCamera", 8080);
    emit startStreamingSignal("RCamera", 8081);
    qDebug() << "Переподключение выполнено";
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        emit stopAllCamerasSignal();
        close();
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::on_takeStereoframeButton_clicked()
{
    emit stereoShotSignal();
}

void MainWindow::on_takeStereoSerialButton_clicked()
{
    isStereoSerail = !isStereoSerail;
    if (isStereoSerail) {
        int shots = 0;
        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [=]() mutable {
            if (shots < 5) {
                emit stereoShotSignal();
                shots++;
            } else {
                timer->stop();
                timer->deleteLater();
            }
        });
        timer->start(500);
    }
}

void MainWindow::on_openStereoProcessingButton_clicked() {
    // Получаем путь к директории текущего приложения
    QString appDir = QCoreApplication::applicationDirPath();

    // Формируем полный путь к .exe
    QString exePath = appDir + "/StereoReconstruction.exe";
    QString exeName = "StereoReconstruction.exe";

    // Проверяем, идет ли уже проверка
    if (m_isCheckingProcess) {
        qDebug() << "Проверка уже выполняется. Игнорируем повторное нажатие.";
        return;
    }

    m_isCheckingProcess = true;

    // Асинхронная проверка с помощью QProcess
    QProcess* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, exePath, exeName](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            qWarning() << "Ошибка выполнения tasklist. Код выхода:" << exitCode;
            m_isCheckingProcess = false;
            process->deleteLater();
            return;
        }

        QByteArray output = process->readAllStandardOutput();
        QString outputStr = QString::fromLocal8Bit(output);

        m_isCheckingProcess = false;
        process->deleteLater();

        // Если в выводе есть имя exe (значит запущена), не запускаем заново
        if (outputStr.contains(exeName, Qt::CaseInsensitive)) {
            return;
        }

        // Запускаем программу в отдельном процессе
        bool started = QProcess::startDetached(exePath);
        if (!started) {
            qWarning() << "Failed to start external program:" << exePath;
        }
    });

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        qWarning() << "Ошибка QProcess:" << error;
        m_isCheckingProcess = false;
        process->deleteLater();
    });

    process->start("tasklist", QStringList() << "/FI" << QString("IMAGENAME eq %1").arg(exeName) << "/NH");
}

void MainWindow::masterSwitch()
{
    masterState = !masterState;
    setMasterButtonState(ui->masterButton, masterState, isPanelHidden);
    emit masterChanged(masterState);
}

void setMasterButtonState(QPushButton *button, const bool masterState, const bool isPanelHidden)
{
    if (masterState) {
        if (!isPanelHidden)
            button->setText("Заблокировать ТНПА");
        QIcon unlocked(":/Resources/Icons/lock_open.ico");
        button->setIcon(unlocked);
    } else {
        if (!isPanelHidden)
            button->setText("Разблокировать ТНПА");
        QIcon locked(":/Resources/Icons/lock_closed.ico");
        button->setIcon(locked);
    }
}

void MainWindow::startRecord() {
    // Проверка на быстрое нажатие (debounce)
    QDateTime now = QDateTime::currentDateTime();
    if (lastRecordToggleTime.isValid() && now < lastRecordToggleTime.addSecs(5)) {
        qDebug() << "Слишком быстрое нажатие кнопки записи";
        return;
    }

    // ui->recordStereoCheckBox->setDisabled(!isRecording);
    isRecording = !isRecording;
    // isStereoRecording = ui->recordStereoCheckBox->isChecked();

    setRecordButtonState(ui->startRecordButton, isRecording, isPanelHidden);

    isStereoRecording = false;

    if (isRecording) {
        emit startRecordingSignal("LCamera", 120, 0, Both);
        if (isStereoRecording) {
            emit startRecordingSignal("RCamera", 120, 0, NoOverlay);
        }
    } else {
        emit stopRecordingSignal("LCamera");
        if (isStereoRecording) {
            emit stopRecordingSignal("RCamera");
        }
    }

    // Обновляем время последнего успешного переключения
    lastRecordToggleTime = now;
}

void setRecordButtonState(QPushButton *button, const bool isRecording, const bool isPanelHidden)
{
    if (isRecording) {
        if (!isPanelHidden)
            button->setText("Остановить запись");
        QIcon icon(":/Resources/Icons/circle_red.ico");
        button->setIcon(icon);
    } else {
        if (!isPanelHidden)
            button->setText("Записать видео");
        QIcon icon(":/Resources/Icons/circle_black.ico");
        button->setIcon(icon);
    }
}

void MainWindow::onDatagramReceived(const QByteArray &data, const QHostAddress &sender, quint16 port)
{
}

void MainWindow::onResize()
{
    if (m_cameraLayout) {
        m_cameraLayout->update();
        // Обновляем геометрию оверлея при изменении размера
        if (m_overlay && m_label) {
            m_overlay->setGeometry(0, 0, m_label->width(), m_label->height());
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    // Обновляем геометрию оверлея при изменении размера окна
    if (m_overlay && m_label) {
        m_overlay->setGeometry(0, 0, m_label->width(), m_label->height());
    }
}

void MainWindow::activeProfileChanged(){
    SettingsManager &settingsManager = SettingsManager::instance();
    // if (settingsManager){
    //     return;
    // }
    QJsonObject profile = profileManager->getProfile();
    QString profileName = profile["profileName"].toString();
    settingsManager.updateLastActiveProfile(profileName);
    settingsDialog->SaveSetting();
}

void MainWindow::settingsChanged(){

}

void MainWindow::updatePID(){


}
void MainWindow::resetAngle(){

}

void MainWindow::onJoystickUpdate(const DualJoystickState &state)
{
    // qDebug() << state;
}

void MainWindow::updateOverlayData(){
    m_overlay->telemetryUpdate(telemetryPacket);
    m_overlay->controlsUpdate(stabEnabled,
                              stabRollEnabled,
                              stabPitchEnabled,
                              stabYawEnabled,
                              stabDepthEnabled,
                              masterState,
                              powerLimit,
                              camAngle,
                              lightsState);
}

void MainWindow::updateMasterFromControl(const bool &masterState){
    MainWindow::masterState = masterState;
    setMasterButtonState(ui->masterButton, masterState, isPanelHidden);
}

float mapValueF(float x, float in_min, float in_max, float out_min, float out_max)
{
    if (in_max == in_min)
        return out_min; // защита от деления на 0

    return float(x - in_min) * (out_max - out_min) / float(in_max - in_min) + out_min;
}

void MainWindow::telemetryReceived(const TelemetryPacket &packet){
    telemetryPacket = packet;
    float tCamAngle = packet.cameraAngle;
    float tCamMin = SettingsManager::instance().getDouble("Cam_angle_minus");
    float tCamMax = SettingsManager::instance().getDouble("Cam_angle_plus");
    camAngle = mapValueF(tCamAngle, tCamMin, tCamMax, -90, 90);
}

void MainWindow::setStabState(){
    stabEnabled = ui->enableStabCheckBox->isChecked();
    if(stabEnabled){
        ui->enableRollStabCheckBox->setEnabled(true);
        ui->enablePitchStabCheckBox->setEnabled(true);
        ui->enableYawStabCheckBox->setEnabled(true);
        ui->enableDepthStabCheckBox->setEnabled(true);
        stabRollEnabled = ui->enableRollStabCheckBox->isChecked();
        stabPitchEnabled = ui->enablePitchStabCheckBox->isChecked();
        stabYawEnabled = ui->enableYawStabCheckBox->isChecked();
        stabDepthEnabled = ui->enableDepthStabCheckBox->isChecked();
    } else {
        ui->enableRollStabCheckBox->setEnabled(false);
        ui->enablePitchStabCheckBox->setEnabled(false);
        ui->enableYawStabCheckBox->setEnabled(false);
        ui->enableDepthStabCheckBox->setEnabled(false);
        stabRollEnabled = false;
        stabPitchEnabled = false;
        stabYawEnabled = false;
        stabDepthEnabled = false;
    }
    emit stabUpdated(stabEnabled, stabRollEnabled, stabPitchEnabled, stabYawEnabled, stabDepthEnabled);
}

void MainWindow::updateLightState(const bool &lightState)
{
    lightsState = lightState;
}
