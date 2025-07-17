#include "camera.h"

Camera::Camera(QStringList& names, QObject* parent) : QObject(parent), m_cameraNames(names), m_reconnectAttempts(0), m_maxReconnectAttempts(5) {
    qDebug() << "Создание объекта Camera";
    memset(&m_deviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    m_checkCameraTimer = new QTimer(this);
    connect(m_checkCameraTimer, &QTimer::timeout, this, &Camera::checkCameras);

    cleanupAllCameras();

    if (checkCameras() != MV_OK) {
        QString errorMsg = "Не удалось инициализировать камеры";
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        return;
    }
}

Camera::~Camera() {
    qDebug() << "Уничтожение объекта Camera...";
    stopAll();
    cleanupAllCameras();

    if (m_checkCameraTimer) {
        m_checkCameraTimer->stop();
        delete m_checkCameraTimer;
        m_checkCameraTimer = nullptr;
    }

    for (size_t i = 0; i < m_cameras.size(); ++i) {
        delete m_cameras[i]->worker;
        delete m_cameras[i]->thread;
        delete m_streamInfos[i]->streamer;
        delete m_streamInfos[i]->streamerThread;
        delete m_recordInfos[i]->recorder;
        delete m_recordInfos[i]->recorderThread;
        delete m_cameras[i];
        delete m_streamInfos[i];
        delete m_recordInfos[i];
    }
    m_cameras.clear();
    m_streamInfos.clear();
    m_recordInfos.clear();
    qDebug() << "Объект Camera уничтожен.";
    qDebug() << "Good Luck!";
}

void Camera::startCamera() {
    start();
}

void Camera::stopAllCameras() {
    stopAll();
}

void Camera::startRecordingSlot(const QString& cameraName, int recordInterval, int storedVideoFilesLimit) {
    startRecording(cameraName, recordInterval, storedVideoFilesLimit);
}

void Camera::stopRecordingSlot(const QString& cameraName) {
    stopRecording(cameraName);
}

void Camera::startStreamingSlot(const QString& cameraName, int port) {
    startStreaming(cameraName, port);
}

void Camera::stopStreamingSlot(const QString& cameraName) {
    stopStreaming(cameraName);
}

void Camera::stereoShotSlot() {
    stereoShot();
}

void Camera::initializeCameras() {
    if (checkCameras() != MV_OK) {
        QString errorMsg = "Не удалось обновить список камер";
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        return;
    }

    for (size_t i = 0; i < m_cameras.size(); ++i) {
        CameraFrameInfo* frameInfo = m_cameras[i];
        StreamFrameInfo* streamInfo = m_streamInfos[i];
        RecordFrameInfo* recordInfo = m_recordInfos[i];
        qDebug() << "Инициализация камеры" << frameInfo->name << "с ID" << frameInfo->id;
        getHandle(frameInfo->id, &frameInfo->handle, frameInfo->name.toStdString());
        streamInfo->id = frameInfo->id;
        recordInfo->id = frameInfo->id;

        if (!frameInfo->handle || frameInfo->id < 0 || frameInfo->id >= (int)m_deviceList.nDeviceNum || !m_deviceList.pDeviceInfo[frameInfo->id]) {
            QString errorMsg = QString("Камера %1 не инициализирована").arg(frameInfo->name);
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
            frameInfo->handle = nullptr;
            continue;
        }

        if (m_deviceList.pDeviceInfo[frameInfo->id]->nTLayerType == MV_GIGE_DEVICE) {
            int nPacketSize = MV_CC_GetOptimalPacketSize(frameInfo->handle);
            if (nPacketSize <= 0) {
                QString errorMsg = QString("Не удалось определить оптимальный размер пакета для %1. Код ошибки: %2")
                                       .arg(frameInfo->name).arg(nPacketSize);
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
            } else {
                int nRet = MV_CC_SetIntValue(frameInfo->handle, "GevSCPSPacketSize", nPacketSize);
                if (nRet != MV_OK) {
                    QString errorMsg = QString("Не удалось установить размер пакета для %1. Ошибка: %2")
                                           .arg(frameInfo->name).arg(nRet);
                    qDebug() << errorMsg;
                    emit errorOccurred("Camera", errorMsg);
                    destroyCameras(frameInfo->handle);
                    frameInfo->handle = nullptr;
                    continue;
                }
                qDebug() << "Установлен размер пакета для" << frameInfo->name << ":" << nPacketSize;
            }
        }

        memset(&frameInfo->frame, 0, sizeof(MV_DISPLAY_FRAME_INFO));

        frameInfo->worker = new CameraWorker(frameInfo, streamInfo, recordInfo);
        frameInfo->thread = new QThread(this);
        frameInfo->worker->moveToThread(frameInfo->thread);
        qDebug() << "Создан поток захвата для камеры" << frameInfo->name;

        recordInfo->recorder = new VideoRecorder(recordInfo);
        recordInfo->recorderThread = new QThread(this);
        recordInfo->recorder->moveToThread(recordInfo->recorderThread);
        recordInfo->recorderThread->start();
        recordInfo->recorderThread->setPriority(QThread::LowPriority);
        qDebug() << "Создан и запущен поток записи для камеры" << frameInfo->name;

        streamInfo->streamer = new VideoStreamer(streamInfo, 0);
        streamInfo->streamerThread = new QThread(this);
        streamInfo->streamer->moveToThread(streamInfo->streamerThread);
        streamInfo->streamerThread->start();
        qDebug() << "Создан и запущен поток стриминга для камеры" << frameInfo->name;

        connect(frameInfo->worker, &CameraWorker::errorOccurred, this, &Camera::errorOccurred);
        connect(recordInfo->recorder, &VideoRecorder::errorOccurred, this, &Camera::errorOccurred);
        connect(streamInfo->streamer, &VideoStreamer::errorOccurred, this, &Camera::errorOccurred);
    }

    bool anyCameraInitialized = false;
    for (const CameraFrameInfo* frameInfo : m_cameras) {
        if (frameInfo->handle) {
            anyCameraInitialized = true;
            qDebug() << "Камера" << frameInfo->name << "успешно инициализирована.";
            break;
        }
    }
    if (!anyCameraInitialized) {
        QString errorMsg = "Не удалось открыть ни одну камеру";
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        return;
    }
}

void Camera::reinitializeCameras() {
    for (size_t i = 0; i < m_cameras.size(); ++i) {
        CameraFrameInfo* frameInfo = m_cameras[i];
        StreamFrameInfo* streamInfo = m_streamInfos[i];
        RecordFrameInfo* recordInfo = m_recordInfos[i];
        qDebug() << "Инициализация камеры" << frameInfo->name << "с ID" << frameInfo->id;
        getHandle(frameInfo->id, &frameInfo->handle, frameInfo->name.toStdString());
        streamInfo->id = frameInfo->id;
        recordInfo->id = frameInfo->id;

        if (!frameInfo->handle || frameInfo->id < 0 || frameInfo->id >= (int)m_deviceList.nDeviceNum || !m_deviceList.pDeviceInfo[frameInfo->id]) {
            QString errorMsg = QString("Камера %1 не инициализирована").arg(frameInfo->name);
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
            frameInfo->handle = nullptr;
            continue;
        }

        if (m_deviceList.pDeviceInfo[frameInfo->id]->nTLayerType == MV_GIGE_DEVICE) {
            int nPacketSize = MV_CC_GetOptimalPacketSize(frameInfo->handle);
            if (nPacketSize <= 0) {
                QString errorMsg = QString("Не удалось определить оптимальный размер пакета для %1. Код ошибки: %2")
                                       .arg(frameInfo->name).arg(nPacketSize);
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
            } else {
                int nRet = MV_CC_SetIntValue(frameInfo->handle, "GevSCPSPacketSize", nPacketSize);
                if (nRet != MV_OK) {
                    QString errorMsg = QString("Не удалось установить размер пакета для %1. Ошибка: %2")
                                           .arg(frameInfo->name).arg(nRet);
                    qDebug() << errorMsg;
                    emit errorOccurred("Camera", errorMsg);
                    destroyCameras(frameInfo->handle);
                    frameInfo->handle = nullptr;
                    continue;
                }
                qDebug() << "Установлен размер пакета для" << frameInfo->name << ":" << nPacketSize;
            }
        }

        memset(&frameInfo->frame, 0, sizeof(MV_DISPLAY_FRAME_INFO));

        frameInfo->worker = new CameraWorker(frameInfo, streamInfo, recordInfo);
        frameInfo->thread = new QThread(this);
        frameInfo->worker->moveToThread(frameInfo->thread);
        qDebug() << "Создан поток захвата для камеры" << frameInfo->name;

        recordInfo->recorder = new VideoRecorder(recordInfo);
        recordInfo->recorderThread = new QThread(this);
        recordInfo->recorder->moveToThread(recordInfo->recorderThread);
        recordInfo->recorderThread->start();
        recordInfo->recorderThread->setPriority(QThread::LowPriority);
        qDebug() << "Создан и запущен поток записи для камеры" << frameInfo->name;

        streamInfo->streamer = new VideoStreamer(streamInfo, 0);
        streamInfo->streamerThread = new QThread(this);
        streamInfo->streamer->moveToThread(streamInfo->streamerThread);
        streamInfo->streamerThread->start();
        qDebug() << "Создан и запущен поток стриминга для камеры" << frameInfo->name;

        connect(frameInfo->worker, &CameraWorker::errorOccurred, this, &Camera::errorOccurred);
        connect(recordInfo->recorder, &VideoRecorder::errorOccurred, this, &Camera::errorOccurred);
        connect(streamInfo->streamer, &VideoStreamer::errorOccurred, this, &Camera::errorOccurred);
    }

    bool anyCameraInitialized = false;
    for (const CameraFrameInfo* frameInfo : m_cameras) {
        if (frameInfo->handle) {
            anyCameraInitialized = true;
            qDebug() << "Камера" << frameInfo->name << "успешно инициализирована.";
            break;
        }
    }
    if (!anyCameraInitialized) {
        QString errorMsg = "Не удалось открыть ни одну камеру";
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        return;
    }
}

void Camera::setCameraNames(const QStringList& names) {
    qDebug() << "Текущие имена камер до изменения:" << m_cameraNames;
    m_cameraNames = names;
    qDebug() << "Установлены новые имена камер:" << m_cameraNames;
}

void Camera::start() {
    qDebug() << "Запуск всех потоков захвата...";
    for (size_t i = 0; i < m_cameras.size(); ++i) {
        CameraFrameInfo* frameInfo = m_cameras[i];
        if (frameInfo->worker && frameInfo->thread) {
            connect(frameInfo->thread, &QThread::started, frameInfo->worker, &CameraWorker::capture, Qt::UniqueConnection);
            connect(frameInfo->worker, &CameraWorker::frameReady, this, [this, frameInfo]() {
                emit frameReady(frameInfo);
            }, Qt::QueuedConnection);
            connect(frameInfo->worker, &CameraWorker::captureFailed, this, &Camera::handleCaptureFailure, Qt::QueuedConnection);
            frameInfo->thread->start();
            qDebug() << "Поток захвата для камеры" << frameInfo->name << "запущен.";
        } else {
            QString errorMsg = QString("Поток захвата для камеры %1 не запущен: не инициализирован").arg(frameInfo->name);
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
        }
    }
}

void Camera::stopAll() {
    qDebug() << "Остановка всех потоков...";
    for (size_t i = 0; i < m_cameras.size(); ++i) {
        CameraFrameInfo* frameInfo = m_cameras[i];
        StreamFrameInfo* streamInfo = m_streamInfos[i];
        RecordFrameInfo* recordInfo = m_recordInfos[i];

        // Остановка объектов
        if (frameInfo->worker) {
            frameInfo->worker->stop();
            qDebug() << "Остановлен worker для камеры" << frameInfo->name;
        }
        if (recordInfo->recorder) {
            recordInfo->recorder->stopRecording();
            qDebug() << "Остановлен recorder для камеры" << frameInfo->name;
            disconnect(recordInfo->recorder, nullptr, nullptr, nullptr);
        }
        if (streamInfo->streamer) {
            streamInfo->streamer->stopStreaming();
            qDebug() << "Остановлен streamer для камеры" << frameInfo->name;
        }

        // Завершение потоков с проверкой
        if (frameInfo->thread && frameInfo->thread->isRunning()) {
            frameInfo->thread->quit();
            if (!frameInfo->thread->wait(5000)) {
                qDebug() << "Поток захвата для" << frameInfo->name << "не завершился, принудительное завершение";
                frameInfo->thread->terminate();
                frameInfo->thread->wait();
            }
        }
        if (recordInfo->recorderThread && recordInfo->recorderThread->isRunning()) {
            recordInfo->recorderThread->quit();
            if (!recordInfo->recorderThread->wait(5000)) {
                qDebug() << "Поток записи для" << frameInfo->name << "не завершился, принудительное завершение";
                recordInfo->recorderThread->terminate();
                recordInfo->recorderThread->wait();
            }
        }
        if (streamInfo->streamerThread && streamInfo->streamerThread->isRunning()) {
            streamInfo->streamerThread->quit();
            if (!streamInfo->streamerThread->wait(5000)) {
                qDebug() << "Поток стриминга для" << frameInfo->name << "не завершился, принудительное завершение";
                streamInfo->streamerThread->terminate();
                streamInfo->streamerThread->wait();
            }
        }

        // Очистка объектов
        delete frameInfo->worker;
        frameInfo->worker = nullptr;
        delete recordInfo->recorder;
        recordInfo->recorder = nullptr;
        delete streamInfo->streamer;
        streamInfo->streamer = nullptr;
    }
    qDebug() << "Все потоки остановлены.";
}

void Camera::startRecording(const QString& cameraName, int recordInterval, int storedVideoFilesLimit) {
    qDebug() << "Попытка запуска записи для камеры" << cameraName;
    bool cameraFound = false;
    for (size_t i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i]->name == cameraName) {
            cameraFound = true;
            RecordFrameInfo* recordInfo = m_recordInfos[i];
            CameraFrameInfo* frameInfo = m_cameras[i];
            if (recordInfo->recorder && recordInfo->recorderThread) {
                qDebug() << "Запуск записи для камеры" << recordInfo->name;
                recordInfo->recorder->setRecordInterval(recordInterval);
                recordInfo->recorder->setStoredVideoFilesLimit(storedVideoFilesLimit);

                // Отключаем старые соединения
                disconnect(recordInfo->recorder, &VideoRecorder::recordingStarted, this, nullptr);
                disconnect(recordInfo->recorder, &VideoRecorder::recordingFinished, this, nullptr);
                disconnect(recordInfo->recorder, &VideoRecorder::recordingFailed, this, nullptr);

                // Новые соединения
                connect(recordInfo->recorderThread, &QThread::started, recordInfo->recorder, &VideoRecorder::startRecording, Qt::UniqueConnection);
                connect(recordInfo->recorder, &VideoRecorder::recordingStarted, this,
                        [this, frameInfo]() {
                            qDebug() << "Запись начата для камеры" << frameInfo->name;
                            emit recordingStarted(frameInfo);
                        }, Qt::QueuedConnection);
                connect(recordInfo->recorder, &VideoRecorder::recordingFinished, this,
                        [this, frameInfo]() {
                            qDebug() << "Запись завершена для камеры" << frameInfo->name;
                            emit recordingFinished(frameInfo);
                        }, Qt::QueuedConnection);
                connect(recordInfo->recorder, &VideoRecorder::recordingFailed, this,
                        &Camera::handleRecordingFailure, Qt::QueuedConnection);
                // Добавлено соединение для frameReady
                connect(frameInfo->worker, &CameraWorker::frameReady, recordInfo->recorder, &VideoRecorder::recordFrame, Qt::QueuedConnection);

                if (!recordInfo->recorderThread->isRunning()) {
                    recordInfo->recorderThread->start();
                    qDebug() << "Поток записи для камеры" << recordInfo->name << "запущен.";
                } else {
                    QMetaObject::invokeMethod(recordInfo->recorder, "startRecording", Qt::QueuedConnection);
                    qDebug() << "Асинхронный вызов startRecording для камеры" << recordInfo->name;
                }
            } else {
                QString errorMsg = QString("Recorder или recorderThread не инициализированы для камеры %1").arg(recordInfo->name);
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
            }
            break;
        }
    }
    if (!cameraFound) {
        QString errorMsg = QString("Камера с именем %1 не найдена").arg(cameraName);
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
    }
}

void Camera::stopRecording(const QString& cameraName) {
    qDebug() << "Попытка остановки записи для камеры" << cameraName;
    for (size_t i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i]->name == cameraName && m_recordInfos[i]->recorder) {
            m_recordInfos[i]->recorder->stopRecording();
            qDebug() << "Запись остановлена для камеры" << m_cameras[i]->name;
            break;
        }
    }
}

void Camera::startStreaming(const QString& cameraName, int port) {
    qDebug() << "Попытка запуска стриминга для камеры" << cameraName << "на порту" << port;
    bool cameraFound = false;
    for (size_t i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i]->name == cameraName) {
            cameraFound = true;
            StreamFrameInfo* streamInfo = m_streamInfos[i];
            if (streamInfo->streamer && streamInfo->streamerThread) {
                streamInfo->streamer->stopStreaming();
                delete streamInfo->streamer;
                streamInfo->streamer = nullptr;

                streamInfo->streamer = new VideoStreamer(streamInfo, port);
                streamInfo->streamer->moveToThread(streamInfo->streamerThread);
                qDebug() << "Запуск стриминга для камеры" << streamInfo->name;
                connect(streamInfo->streamerThread, &QThread::started, streamInfo->streamer, &VideoStreamer::startStreaming, Qt::UniqueConnection);
                connect(streamInfo->streamer, &VideoStreamer::streamingStarted, this, [this, frameInfo = m_cameras[i]]() {
                    qDebug() << "Стриминг начат для камеры" << frameInfo->name;
                    emit streamingStarted(frameInfo);
                }, Qt::QueuedConnection);
                connect(streamInfo->streamer, &VideoStreamer::streamingFinished, this, [this, frameInfo = m_cameras[i]]() {
                    qDebug() << "Стриминг завершен для камеры" << frameInfo->name;
                    emit streamingFinished(frameInfo);
                }, Qt::QueuedConnection);
                connect(streamInfo->streamer, &VideoStreamer::streamingFailed, this, &Camera::handleStreamingFailure, Qt::QueuedConnection);
                if (!streamInfo->streamerThread->isRunning()) {
                    streamInfo->streamerThread->start();
                    qDebug() << "Поток стриминга для камеры" << streamInfo->name << "запущен.";
                } else {
                    QMetaObject::invokeMethod(streamInfo->streamer, "startStreaming", Qt::QueuedConnection);
                    qDebug() << "Асинхронный вызов startStreaming для камеры" << streamInfo->name;
                }
            } else {
                QString errorMsg = QString("Streamer или streamerThread не инициализированы для камеры %1").arg(streamInfo->name);
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
            }
            break;
        }
    }
    if (!cameraFound) {
        QString errorMsg = QString("Камера с именем %1 не найдена").arg(cameraName);
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
    }
}

void Camera::stopStreaming(const QString& cameraName) {
    qDebug() << "Попытка остановки стриминга для камеры" << cameraName;
    for (size_t i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i]->name == cameraName && m_streamInfos[i]->streamer) {
            QMetaObject::invokeMethod(m_streamInfos[i]->streamer, "stopStreaming", Qt::QueuedConnection);
            qDebug() << "Стриминг остановлен для камеры" << m_cameras[i]->name;
            break;
        }
    }
}

void Camera::stereoShot() {
    qDebug() << "Вызван stereoShot, формат сохранения: PNG";
    StreamFrameInfo* lCameraInfo = nullptr;
    StreamFrameInfo* rCameraInfo = nullptr;

    for (size_t i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i]->name == "LCamera") lCameraInfo = m_streamInfos[i];
        else if (m_cameras[i]->name == "RCamera") rCameraInfo = m_streamInfos[i];
    }

    if (!lCameraInfo || !rCameraInfo) {
        QString errorMsg = "Не найдены обе камеры LCamera и RCamera";
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        emit stereoShotFailed(errorMsg);
        return;
    }

    cv::Mat lFrame, rFrame;
    {
        QMutexLocker lLocker(lCameraInfo->mutex);
        QMutexLocker rLocker(rCameraInfo->mutex);

        if (!lCameraInfo->img.empty()) {
            lFrame = lCameraInfo->img.clone();
        }
        if (!rCameraInfo->img.empty()) {
            rFrame = rCameraInfo->img.clone();
        }
    }

    if (lFrame.empty() || rFrame.empty()) {
        cv::cvtColor(lFrame, lFrame, cv::COLOR_BGR2RGB);
        cv::cvtColor(rFrame, rFrame, cv::COLOR_BGR2RGB);
        QString errorMsg = QString("Один или оба кадра пусты (LCamera: %1, RCamera: %2)")
                               .arg(lFrame.empty() ? "пуст" : "не пуст")
                               .arg(rFrame.empty() ? "пуст" : "не пуст");
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        emit stereoShotFailed(errorMsg);
        return;
    }

    std::filesystem::path stereoDirectory = std::filesystem::current_path() / "stereo";
    std::filesystem::path lDirectory = stereoDirectory / "L";
    std::filesystem::path rDirectory = stereoDirectory / "R";

    try {
        if (!std::filesystem::exists(stereoDirectory)) {
            if (!std::filesystem::create_directory(stereoDirectory)) {
                QString errorMsg = "Не удалось создать директорию stereo";
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
                emit stereoShotFailed(errorMsg);
                return;
            }
        }
        if (!std::filesystem::exists(lDirectory)) {
            if (!std::filesystem::create_directory(lDirectory)) {
                QString errorMsg = "Не удалось создать директорию stereo/L";
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
                emit stereoShotFailed(errorMsg);
                return;
            }
        }
        if (!std::filesystem::exists(rDirectory)) {
            if (!std::filesystem::create_directory(rDirectory)) {
                QString errorMsg = "Не удалось создать директорию stereo/R";
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
                emit stereoShotFailed(errorMsg);
                return;
            }
        }

        std::filesystem::perms perms = std::filesystem::status(stereoDirectory).permissions();
        if ((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none) {
            QString errorMsg = "Нет прав на запись в директорию stereo";
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
            emit stereoShotFailed(errorMsg);
            return;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        QString errorMsg = QString("Ошибка файловой системы при создании директорий: %1").arg(e.what());
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        emit stereoShotFailed(errorMsg);
        return;
    }

    auto now = std::time(nullptr);
    std::tm timeInfo;
#ifdef _MSC_VER
    localtime_s(&timeInfo, &now);
#else
    timeInfo = *std::localtime(&now);
#endif
    std::stringstream ss;
    ss << std::put_time(&timeInfo, "%Y%m%d_%H%M%S");
    std::string timestamp = ss.str();

    std::string lFileName = "LCamera_" + timestamp + ".png";
    std::string rFileName = "RCamera_" + timestamp + ".png";
    std::string lFilePath = (lDirectory / lFileName).string();
    std::string rFilePath = (rDirectory / rFileName).string();

    std::vector<int> compression_params;
    compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(0);

    try {
        if (!cv::imwrite(lFilePath, lFrame, compression_params)) {
            QString errorMsg = QString("Не удалось сохранить кадр LCamera в %1").arg(QString::fromStdString(lFilePath));
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
            emit stereoShotFailed(errorMsg);
            return;
        }
        if (!cv::imwrite(rFilePath, rFrame, compression_params)) {
            QString errorMsg = QString("Не удалось сохранить кадр RCamera в %1").arg(QString::fromStdString(rFilePath));
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
            emit stereoShotFailed(errorMsg);
            return;
        }
        qDebug() << "Стереокадры успешно сохранены: " << QString::fromStdString(lFilePath) << ", " << QString::fromStdString(rFilePath);
        emit stereoShotSaved(QString::fromStdString(lFilePath) + ";" + QString::fromStdString(rFilePath));
    } catch (const cv::Exception& e) {
        QString errorMsg = QString("Ошибка сохранения стереокадров: %1").arg(e.what());
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        emit stereoShotFailed(errorMsg);
        return;
    }
}

const QList<CameraFrameInfo*>& Camera::getCameras() const {
    return m_cameras;
}

QStringList Camera::getCameraNames() const {
    return m_cameraNames;
}

int Camera::checkCameras() {
    int nRet = MV_OK;
    memset(&m_deviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &m_deviceList);

    if (nRet != MV_OK || m_deviceList.nDeviceNum == 0) {
        QString errorMsg = QString("Устройства не найдены или не удалось выполнить перечисление. Ошибка: %1").arg(nRet);
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);

        if (!m_checkCameraTimer->isActive()) {
            qDebug() << "Запуск таймера для повторной проверки камер через 10 секунд";
            m_checkCameraTimer->start(10000);
        }
        return nRet;
    }

    if (m_checkCameraTimer->isActive()) {
        qDebug() << "Камеры найдены, остановка таймера проверки";
        m_checkCameraTimer->stop();
    }

    qDebug() << "Найдено" << m_deviceList.nDeviceNum << "устройств";
    qDebug() << "Текущий список имен камер:" << m_cameraNames;

    m_cameras.clear();
    m_streamInfos.clear();
    m_recordInfos.clear();
    for (unsigned int i = 0; i < m_deviceList.nDeviceNum; i++) {
        if (!m_deviceList.pDeviceInfo[i]) continue;
        std::stringstream cameraName;
        cameraName << m_deviceList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.chUserDefinedName;
        QString camName = QString::fromStdString(cameraName.str());
        qDebug() << "Устройство" << i << "Имя:" << camName
                 << "Серийный номер:" << m_deviceList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.chSerialNumber;

        if (m_cameraNames.contains(camName)) {
            CameraFrameInfo* frameInfo = new CameraFrameInfo();
            StreamFrameInfo* streamInfo = new StreamFrameInfo();
            RecordFrameInfo* recordInfo = new RecordFrameInfo();
            frameInfo->name = camName;
            frameInfo->id = i;
            streamInfo->name = camName;
            streamInfo->id = i;
            recordInfo->name = camName;
            recordInfo->id = i;
            m_cameras.append(frameInfo);
            m_streamInfos.append(streamInfo);
            m_recordInfos.append(recordInfo);
            qDebug() << "Добавлена камера" << camName << "с ID" << i;
        } else {
            qDebug() << "Камера" << camName << "пропущена, так как отсутствует в m_cameraNames";
        }
    }

    if (m_cameras.isEmpty()) {
        QString errorMsg = QString("Не найдено ни одной камеры из списка: %1").arg(m_cameraNames.join(", "));
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);

        if (!m_checkCameraTimer->isActive()) {
            qDebug() << "Запуск таймера для повторной проверки камер через 10 секунд";
            m_checkCameraTimer->start(10000);
        }
        return -1;
    }

    qDebug() << "Камеры найдены, вызов reconnectCameras()";
    cleanupAllCameras();
    reinitializeCameras();
    start();
    QThread::msleep(5000);
    emit reconnectDone(this);

    QString sMsg = QString("Инициализированы камеры: %1").arg(m_cameras.size());
    emit greatSuccess("Camera", sMsg);

    return MV_OK;
}

int Camera::destroyCameras(void* handle) {
    int nRet = MV_OK;
    if (handle) {
        nRet = MV_CC_StopGrabbing(handle);
        if (nRet != MV_OK && nRet != -2147483648 && nRet != -2147483645 && nRet != -2147483133) {
            QString errorMsg = QString("Не удалось остановить захват. Ошибка: %1").arg(nRet);
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
        }
        nRet = MV_CC_CloseDevice(handle);
        if (nRet != MV_OK && nRet != -2147483648 && nRet != -2147483645) {
            QString errorMsg = QString("Не удалось закрыть устройство. Ошибка: %1").arg(nRet);
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
        }
        nRet = MV_CC_DestroyHandle(handle);
        if (nRet != MV_OK && nRet != -2147483648) {
            QString errorMsg = QString("Не удалось уничтожить дескриптор. Ошибка: %1").arg(nRet);
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
        }
        QThread::msleep(2000);
    }
    return nRet;
}

void Camera::getHandle(unsigned int cameraID, void** handle, const std::string& cameraName) {
    qDebug() << "Получение дескриптора для камеры" << cameraName.c_str() << "с ID" << cameraID;
    int nRet = MV_OK;

    if (cameraID >= m_deviceList.nDeviceNum || !m_deviceList.pDeviceInfo[cameraID]) {
        QString errorMsg = QString("Неверный ID камеры: %1 для %2").arg(cameraID).arg(cameraName.c_str());
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        *handle = nullptr;
        return;
    }

    qDebug() << "Используется ID камеры:" << cameraID
             << "Имя:" << cameraName.c_str()
             << "Серийный номер:" << m_deviceList.pDeviceInfo[cameraID]->SpecialInfo.stGigEInfo.chSerialNumber;

    if (m_deviceList.pDeviceInfo[cameraID]->nTLayerType == MV_GIGE_DEVICE) {
        unsigned int currentIP = m_deviceList.pDeviceInfo[cameraID]->SpecialInfo.stGigEInfo.nCurrentIp;
        unsigned char* ipBytes = reinterpret_cast<unsigned char*>(&currentIP);
        qDebug() << "GigE камера. IP:" << currentIP
                 << QString(" (%1.%2.%3.%4)").arg(ipBytes[0]).arg(ipBytes[1]).arg(ipBytes[2]).arg(ipBytes[3])
                 << "Маска подсети:" << m_deviceList.pDeviceInfo[cameraID]->SpecialInfo.stGigEInfo.nCurrentSubNetMask
                 << "Шлюз:" << m_deviceList.pDeviceInfo[cameraID]->SpecialInfo.stGigEInfo.nDefultGateWay;
        if (m_usedIPs.find(currentIP) != m_usedIPs.end()) {
            QString errorMsg = QString("Обнаружен IP-конфликт! Камера %1 использует IP, уже занятый другой камерой")
                                   .arg(cameraName.c_str());
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
        } else {
            m_usedIPs.insert(currentIP);
        }
    }

    nRet = MV_CC_CreateHandle(handle, m_deviceList.pDeviceInfo[cameraID]);
    if (nRet != MV_OK) {
        QString errorMsg = QString("Не удалось создать дескриптор для %1. Ошибка: %2")
                               .arg(cameraName.c_str()).arg(nRet);
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        *handle = nullptr;
        return;
    }

    int retryCount = 5;
    for (int i = 0; i < retryCount; ++i) {
        nRet = MV_CC_OpenDevice(*handle);
        if (nRet == MV_OK) {
            qDebug() << cameraName.c_str() << "успешно открыта на попытке" << (i + 1);

            MVCC_ENUMVALUE pixelFormat = {0};
            nRet = MV_CC_GetEnumValue(*handle, "PixelFormat", &pixelFormat);
            if (nRet == MV_OK) {
                qDebug() << "Текущий формат пикселей для" << cameraName.c_str() << ":" << pixelFormat.nCurValue;
            } else {
                QString errorMsg = QString("Не удалось получить текущий формат пикселей для %1. Ошибка: %2")
                                       .arg(cameraName.c_str()).arg(nRet);
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
            }

            nRet = MV_CC_SetPixelFormat(*handle, PixelType_Gvsp_BayerRG8);
            if (nRet != MV_OK) {
                QString errorMsg = QString("Не удалось установить формат пикселей для %1. Ошибка: %2")
                                       .arg(cameraName.c_str()).arg(nRet);
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
            }

            return;
        }
        qDebug() << "Не удалось открыть устройство для" << cameraName.c_str() << "Ошибка:" << nRet
                 << "Повтор" << (i + 1) << "из" << retryCount;
        if (nRet == MV_E_ACCESS_DENIED) {
            QString errorMsg = QString("Ошибка доступа (MV_E_ACCESSDENIED) для %1. Возможно, камера занята другим приложением")
                                   .arg(cameraName.c_str());
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
            destroyCameras(*handle);
            QThread::msleep(15000);
            nRet = MV_CC_CreateHandle(handle, m_deviceList.pDeviceInfo[cameraID]);
            if (nRet != MV_OK) {
                QString errorMsg = QString("Не удалось пересоздать дескриптор для %1. Ошибка: %2")
                                       .arg(cameraName.c_str()).arg(nRet);
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
                *handle = nullptr;
                return;
            }
        } else {
            QString errorMsg = QString("Не удалось открыть устройство для %1 после %2 попыток. Ошибка: %3")
                                   .arg(cameraName.c_str()).arg(retryCount).arg(nRet);
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
            destroyCameras(*handle);
            *handle = nullptr;
            return;
        }
    }

    QString errorMsg = QString("Не удалось открыть устройство для %1 после %2 попыток. Ошибка: %3")
                           .arg(cameraName.c_str()).arg(retryCount).arg(nRet);
    qDebug() << errorMsg;
    emit errorOccurred("Camera", errorMsg);
    destroyCameras(*handle);
    *handle = nullptr;
}

void Camera::cleanupAllCameras() {
    qDebug() << "Очистка всех ресурсов камер...";
    for (CameraFrameInfo* frameInfo : m_cameras) {
        destroyCameras(frameInfo->handle);
        frameInfo->handle = nullptr;
    }
    m_usedIPs.clear();

    if (m_checkCameraTimer->isActive()) {
        qDebug() << "Остановка таймера проверки камер во время очистки";
        m_checkCameraTimer->stop();
    }

    QThread::msleep(5000);
    qDebug() << "Все ресурсы камер очищены.";
}

void Camera::reconnectCameras() {
    qDebug() << "Переподключение камер...";
    stopAll();
    cleanupAllCameras();
    reinitializeCameras();
    start();
    m_reconnectAttempts = 0;
    QThread::msleep(5000);
    emit reconnectDone(this);
    qDebug() << "Переподключение камер завершено.";
}

void Camera::handleCaptureFailure(const QString& reason) {
    QString errorMsg = QString("Ошибка захвата: %1").arg(reason);
    qDebug() << errorMsg;
    emit errorOccurred("Camera", errorMsg);

    if (reason.contains("Не удалось получить буфер изображения")) {
        qDebug() << "Камера, возможно, отключена. Остановка всех операций и запуск таймера для периодической проверки.";
        stopAll();
        if (!m_checkCameraTimer->isActive()) {
            m_checkCameraTimer->start(10000);
        }
    } else {
        qDebug() << "Неизвестная ошибка захвата, остановка всех потоков";
        stopAll();
        emit finished();
    }
}

void Camera::handleRecordingFailure(const QString& reason) {
    QString errorMsg = QString("Ошибка записи видео: %1").arg(reason);
    qDebug() << errorMsg;
    emit errorOccurred("Camera", errorMsg);
    stopAll();
    emit finished();
}

void Camera::handleStreamingFailure(const QString& reason) {
    QString errorMsg = QString("Ошибка стриминга видео: %1").arg(reason);
    qDebug() << errorMsg;
    emit errorOccurred("Camera", errorMsg);
    stopAll();
    emit finished();
}
