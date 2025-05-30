#include "camera.h"

Camera::Camera(QObject* parent) : QObject(parent), m_reconnectAttempts(0), m_maxReconnectAttempts(5) {
    qDebug() << "Создание объекта Camera";
    memset(&m_deviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    m_cameraNames = {"LCamera", "RCamera"};

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
        delete m_videoInfos[i]->processor;
        delete m_videoInfos[i]->processorThread;
        delete m_videoInfos[i]->recorder;
        delete m_videoInfos[i]->recorderThread;
        delete m_videoInfos[i]->streamer;
        delete m_videoInfos[i]->streamerThread;
        delete m_cameras[i];
        delete m_videoInfos[i];
    }
    m_cameras.clear();
    m_videoInfos.clear();
    qDebug() << "Объект Camera уничтожен.";
    qDebug() << "Good Luck!";
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
        CameraVideoFrameInfo* videoInfo = m_videoInfos[i];
        qDebug() << "Инициализация камеры" << frameInfo->name << "с ID" << frameInfo->id;
        getHandle(frameInfo->id, &frameInfo->handle, frameInfo->name.toStdString());
        videoInfo->handle = frameInfo->handle;
        videoInfo->id = frameInfo->id;

        if (!frameInfo->handle || frameInfo->id < 0 || frameInfo->id >= (int)m_deviceList.nDeviceNum || !m_deviceList.pDeviceInfo[frameInfo->id]) {
            QString errorMsg = QString("Камера %1 не инициализирована").arg(frameInfo->name);
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
            frameInfo->handle = nullptr;
            videoInfo->handle = nullptr;
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
                    videoInfo->handle = nullptr;
                    continue;
                }
                qDebug() << "Установлен размер пакета для" << frameInfo->name << ":" << nPacketSize;
            }
        }

        memset(&frameInfo->frame, 0, sizeof(MV_DISPLAY_FRAME_INFO));

        frameInfo->worker = new CameraWorker(frameInfo, videoInfo);
        frameInfo->thread = new QThread(this);
        frameInfo->worker->moveToThread(frameInfo->thread);
        qDebug() << "Создан поток захвата для камеры" << frameInfo->name;

        videoInfo->processor = new FrameProcessor(videoInfo);
        videoInfo->processorThread = new QThread(this);
        videoInfo->processor->moveToThread(videoInfo->processorThread);
        videoInfo->processorThread->start();
        qDebug() << "Создан и запущен поток обработки для камеры" << frameInfo->name;

        videoInfo->recorder = new VideoRecorder(videoInfo);
        videoInfo->recorderThread = new QThread(this);
        videoInfo->recorder->moveToThread(videoInfo->recorderThread);
        videoInfo->recorderThread->start();
        videoInfo->recorderThread->setPriority(QThread::LowPriority);
        qDebug() << "Создан и запущен поток записи для камеры" << frameInfo->name;

        videoInfo->streamer = new VideoStreamer(videoInfo, 0);
        videoInfo->streamerThread = new QThread(this);
        videoInfo->streamer->moveToThread(videoInfo->streamerThread);
        videoInfo->streamerThread->start();
        qDebug() << "Создан и запущен поток стриминга для камеры" << frameInfo->name;

        connect(frameInfo->worker, &CameraWorker::errorOccurred, this, &Camera::errorOccurred);
        connect(videoInfo->processor, &FrameProcessor::errorOccurred, this, &Camera::errorOccurred);
        connect(videoInfo->recorder, &VideoRecorder::errorOccurred, this, &Camera::errorOccurred);
        connect(videoInfo->streamer, &VideoStreamer::errorOccurred, this, &Camera::errorOccurred);
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
        CameraVideoFrameInfo* videoInfo = m_videoInfos[i];
        qDebug() << "Инициализация камеры" << frameInfo->name << "с ID" << frameInfo->id;
        getHandle(frameInfo->id, &frameInfo->handle, frameInfo->name.toStdString());
        videoInfo->handle = frameInfo->handle;
        videoInfo->id = frameInfo->id;

        if (!frameInfo->handle || frameInfo->id < 0 || frameInfo->id >= (int)m_deviceList.nDeviceNum || !m_deviceList.pDeviceInfo[frameInfo->id]) {
            QString errorMsg = QString("Камера %1 не инициализирована").arg(frameInfo->name);
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
            frameInfo->handle = nullptr;
            videoInfo->handle = nullptr;
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
                    videoInfo->handle = nullptr;
                    continue;
                }
                qDebug() << "Установлен размер пакета для" << frameInfo->name << ":" << nPacketSize;
            }
        }

        memset(&frameInfo->frame, 0, sizeof(MV_DISPLAY_FRAME_INFO));

        frameInfo->worker = new CameraWorker(frameInfo, videoInfo);
        frameInfo->thread = new QThread(this);
        frameInfo->worker->moveToThread(frameInfo->thread);
        qDebug() << "Создан поток захвата для камеры" << frameInfo->name;

        videoInfo->processor = new FrameProcessor(videoInfo);
        videoInfo->processorThread = new QThread(this);
        videoInfo->processor->moveToThread(videoInfo->processorThread);
        videoInfo->processorThread->start();
        qDebug() << "Создан и запущен поток обработки для камеры" << frameInfo->name;

        videoInfo->recorder = new VideoRecorder(videoInfo);
        videoInfo->recorderThread = new QThread(this);
        videoInfo->recorder->moveToThread(videoInfo->recorderThread);
        videoInfo->recorderThread->start();
        videoInfo->recorderThread->setPriority(QThread::LowPriority);
        qDebug() << "Создан и запущен поток записи для камеры" << frameInfo->name;

        videoInfo->streamer = new VideoStreamer(videoInfo, 0);
        videoInfo->streamerThread = new QThread(this);
        videoInfo->streamer->moveToThread(videoInfo->streamerThread);
        videoInfo->streamerThread->start();
        qDebug() << "Создан и запущен поток стриминга для камеры" << frameInfo->name;

        connect(frameInfo->worker, &CameraWorker::errorOccurred, this, &Camera::errorOccurred);
        connect(videoInfo->processor, &FrameProcessor::errorOccurred, this, &Camera::errorOccurred);
        connect(videoInfo->recorder, &VideoRecorder::errorOccurred, this, &Camera::errorOccurred);
        connect(videoInfo->streamer, &VideoStreamer::errorOccurred, this, &Camera::errorOccurred);
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
        CameraVideoFrameInfo* videoInfo = m_videoInfos[i];
        if (frameInfo->worker && frameInfo->thread) {
            connect(frameInfo->thread, &QThread::started, frameInfo->worker, &CameraWorker::capture, Qt::UniqueConnection);
            connect(frameInfo->worker, &CameraWorker::frameReady, this, [this, frameInfo]() {
                emit frameReady(frameInfo);
            }, Qt::QueuedConnection);
            connect(frameInfo->worker, &CameraWorker::frameDataReady, videoInfo->processor, &FrameProcessor::processFrame, Qt::QueuedConnection);
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
        CameraVideoFrameInfo* videoInfo = m_videoInfos[i];

        // Остановка объектов
        if (frameInfo->worker) {
            frameInfo->worker->stop();
            qDebug() << "Остановлен worker для камеры" << frameInfo->name;
        }
        if (videoInfo->recorder) {
            videoInfo->recorder->stopRecording();
            qDebug() << "Остановлен recorder для камеры" << frameInfo->name;
            // Отключение всех сигналов и слотов для recorder
            disconnect(videoInfo->recorder, nullptr, nullptr, nullptr);
        }
        if (videoInfo->streamer) {
            videoInfo->streamer->stopStreaming();
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
        if (videoInfo->processorThread && videoInfo->processorThread->isRunning()) {
            videoInfo->processorThread->quit();
            if (!videoInfo->processorThread->wait(5000)) {
                qDebug() << "Поток обработки для" << frameInfo->name << "не завершился, принудительное завершение";
                videoInfo->processorThread->terminate();
                videoInfo->processorThread->wait();
            }
        }
        if (videoInfo->recorderThread && videoInfo->recorderThread->isRunning()) {
            videoInfo->recorderThread->quit();
            if (!videoInfo->recorderThread->wait(5000)) {
                qDebug() << "Поток записи для" << frameInfo->name << "не завершился, принудительное завершение";
                videoInfo->recorderThread->terminate();
                videoInfo->recorderThread->wait();
            }
        }
        if (videoInfo->streamerThread && videoInfo->streamerThread->isRunning()) {
            videoInfo->streamerThread->quit();
            if (!videoInfo->streamerThread->wait(5000)) {
                qDebug() << "Поток стриминга для" << frameInfo->name << "не завершился, принудительное завершение";
                videoInfo->streamerThread->terminate();
                videoInfo->streamerThread->wait();
            }
        }

        // Очистка объектов
        delete frameInfo->worker;
        frameInfo->worker = nullptr;
        delete videoInfo->processor;
        videoInfo->processor = nullptr;
        delete videoInfo->recorder;
        videoInfo->recorder = nullptr;
        delete videoInfo->streamer;
        videoInfo->streamer = nullptr;
    }
    qDebug() << "Все потоки остановлены.";
}

void Camera::startRecording(const QString& cameraName, int recordInterval,  int storedVideoFilesLimit) {
    qDebug() << "Попытка запуска записи для камеры" << cameraName;
    bool cameraFound = false;
    for (size_t i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i]->name == cameraName) {
            cameraFound = true;
            CameraVideoFrameInfo* videoInfo = m_videoInfos[i];
            if (videoInfo->recorder && videoInfo->recorderThread) {
                qDebug() << "Запуск записи для камеры" << videoInfo->name;
                videoInfo->recorder->setRecordInterval(recordInterval);
                videoInfo->recorder->setStoredVideoFilesLimit(storedVideoFilesLimit);
                connect(videoInfo->recorderThread, &QThread::started, videoInfo->recorder, &VideoRecorder::startRecording, Qt::UniqueConnection);
                connect(videoInfo->recorder, &VideoRecorder::recordingStarted, this, [this, frameInfo = m_cameras[i]]() {
                    qDebug() << "Запись начата для камеры" << frameInfo->name;
                    emit recordingStarted(frameInfo);
                }, Qt::QueuedConnection);
                connect(videoInfo->recorder, &VideoRecorder::recordingFinished, this, [this, frameInfo = m_cameras[i]]() {
                    qDebug() << "Запись завершена для камеры" << frameInfo->name;
                    emit recordingFinished(frameInfo);
                }, Qt::QueuedConnection);
                connect(videoInfo->recorder, &VideoRecorder::recordingFailed, this, &Camera::handleRecordingFailure, Qt::QueuedConnection);
                if (!videoInfo->recorderThread->isRunning()) {
                    videoInfo->recorderThread->start();
                    qDebug() << "Поток записи для камеры" << videoInfo->name << "запущен.";
                } else {
                    QMetaObject::invokeMethod(videoInfo->recorder, "startRecording", Qt::QueuedConnection);
                    qDebug() << "Асинхронный вызов startRecording для камеры" << videoInfo->name;
                }
            } else {
                QString errorMsg = QString("Recorder или recorderThread не инициализированы для камеры %1").arg(videoInfo->name);
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
        if (m_cameras[i]->name == cameraName && m_videoInfos[i]->recorder) {
            m_videoInfos[i]->recorder->stopRecording();
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
            CameraVideoFrameInfo* videoInfo = m_videoInfos[i];
            if (videoInfo->streamer && videoInfo->streamerThread) {
                // Останавливаем и удаляем старый streamer
                videoInfo->streamer->stopStreaming();
                delete videoInfo->streamer;
                videoInfo->streamer = nullptr;

                // Создаем новый streamer
                videoInfo->streamer = new VideoStreamer(videoInfo, port);
                videoInfo->streamer->moveToThread(videoInfo->streamerThread);
                qDebug() << "Запуск стриминга для камеры" << videoInfo->name;
                connect(videoInfo->streamerThread, &QThread::started, videoInfo->streamer, &VideoStreamer::startStreaming, Qt::UniqueConnection);
                connect(videoInfo->streamer, &VideoStreamer::streamingStarted, this, [this, frameInfo = m_cameras[i]]() {
                    qDebug() << "Стриминг начат для камеры" << frameInfo->name;
                    emit streamingStarted(frameInfo);
                }, Qt::QueuedConnection);
                connect(videoInfo->streamer, &VideoStreamer::streamingFinished, this, [this, frameInfo = m_cameras[i]]() {
                    qDebug() << "Стриминг завершен для камеры" << frameInfo->name;
                    emit streamingFinished(frameInfo);
                }, Qt::QueuedConnection);
                connect(videoInfo->streamer, &VideoStreamer::streamingFailed, this, &Camera::handleStreamingFailure, Qt::QueuedConnection);
                if (!videoInfo->streamerThread->isRunning()) {
                    videoInfo->streamerThread->start();
                    qDebug() << "Поток стриминга для камеры" << videoInfo->name << "запущен.";
                } else {
                    QMetaObject::invokeMethod(videoInfo->streamer, "startStreaming", Qt::QueuedConnection);
                    qDebug() << "Асинхронный вызов startStreaming для камеры" << videoInfo->name;
                }
            } else {
                QString errorMsg = QString("Streamer или streamerThread не инициализированы для камеры %1").arg(videoInfo->name);
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
        if (m_cameras[i]->name == cameraName && m_videoInfos[i]->streamer) {
            QMetaObject::invokeMethod(m_videoInfos[i]->streamer, "stopStreaming", Qt::QueuedConnection);
            qDebug() << "Стриминг остановлен для камеры" << m_cameras[i]->name;
            break;
        }
    }
}

void Camera::stereoShot() {
    qDebug() << "Вызван stereoShot, формат сохранения: PNG";
    CameraVideoFrameInfo* lCameraInfo = nullptr;
    CameraVideoFrameInfo* rCameraInfo = nullptr;

    for (size_t i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i]->name == "LCamera") lCameraInfo = m_videoInfos[i];
        else if (m_cameras[i]->name == "RCamera") rCameraInfo = m_videoInfos[i];
    }

    if (!lCameraInfo || !rCameraInfo) {
        qDebug() << "Не найдены обе камеры LCamera и RCamera";
        emit stereoShotFailed("Не найдены камеры");
        return;
    }

    cv::Mat lFrame, rFrame;
    if (lCameraInfo->mutex->tryLock(1000) && rCameraInfo->mutex->tryLock(1000)) {
        if (!lCameraInfo->frameBuffer.empty()) lFrame = lCameraInfo->frameBuffer[0].clone();
        if (!rCameraInfo->frameBuffer.empty()) rFrame = rCameraInfo->frameBuffer[0].clone();
        lCameraInfo->mutex->unlock();
        rCameraInfo->mutex->unlock();
    } else {
        qDebug() << "Не удалось заблокировать мьютексы для доступа к буферам";
        emit stereoShotFailed("Тайм-аут блокировки мьютексов");
        return;
    }

    if (lFrame.empty() || rFrame.empty()) {
        qDebug() << "Один или оба кадра пусты";
        emit stereoShotFailed("Кадры пусты");
        return;
    }
    for (size_t i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i]->name == "LCamera") {
            lCameraInfo = m_videoInfos[i];
        } else if (m_cameras[i]->name == "RCamera") {
            rCameraInfo = m_videoInfos[i];
        }
    }

    if (!lCameraInfo || !rCameraInfo) {
        QString errorMsg = "Не найдены обе камеры LCamera и RCamera";
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        emit stereoShotFailed(errorMsg);
        return;
    }

    {
        QMutexLocker lLocker(lCameraInfo->mutex);
        QMutexLocker rLocker(rCameraInfo->mutex);

        if (!lCameraInfo->frameBuffer.empty() && !lCameraInfo->frameBuffer[0].empty()) {
            lFrame = lCameraInfo->frameBuffer[0].clone();
        }
        if (!rCameraInfo->frameBuffer.empty() && !rCameraInfo->frameBuffer[0].empty()) {
            rFrame = rCameraInfo->frameBuffer[0].clone();
        }
    }

    if (lFrame.empty() || rFrame.empty()) {
        QString errorMsg = QString("Один или оба кадра пусты (LCamera: %1, RCamera: %2)")
                               .arg(lFrame.empty() ? "пуст" : "не пуст")
                               .arg(rFrame.empty() ? "пуст" : "не пуст");
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);
        emit stereoShotFailed(errorMsg);
        return;
    }

    std::filesystem::path pathToStereoDirectory = std::filesystem::current_path() / "stereo";
    try {
        if (!std::filesystem::exists(pathToStereoDirectory)) {
            if (!std::filesystem::create_directory(pathToStereoDirectory)) {
                QString errorMsg = "Не удалось создать директорию stereo";
                qDebug() << errorMsg;
                emit errorOccurred("Camera", errorMsg);
                emit stereoShotFailed(errorMsg);
                return;
            }
        }
        std::filesystem::perms perms = std::filesystem::status(pathToStereoDirectory).permissions();
        if ((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none) {
            QString errorMsg = "Нет прав на запись в директорию stereo";
            qDebug() << errorMsg;
            emit errorOccurred("Camera", errorMsg);
            emit stereoShotFailed(errorMsg);
            return;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        QString errorMsg = QString("Ошибка файловой системы при создании директории stereo: %1").arg(e.what());
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
    std::string lFilePath = (pathToStereoDirectory / lFileName).string();
    std::string rFilePath = (pathToStereoDirectory / rFileName).string();

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
        qDebug() << "Стереокадры успешно сохранены: LCamera, " << "RCamera";
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

        // Запуск таймера для повторной проверки через 30 секунд
        if (!m_checkCameraTimer->isActive()) {
            qDebug() << "Запуск таймера для повторной проверки камер через 30 секунд";
            m_checkCameraTimer->start(30000); // 30 секунд
        }
        return nRet;
    }

    // Остановка таймера, если он был запущен
    if (m_checkCameraTimer->isActive()) {
        qDebug() << "Камеры найдены, остановка таймера проверки";
        m_checkCameraTimer->stop();
    }

    qDebug() << "Найдено" << m_deviceList.nDeviceNum << "устройств";
    qDebug() << "Текущий список имен камер:" << m_cameraNames;

    m_cameras.clear();
    m_videoInfos.clear();
    for (unsigned int i = 0; i < m_deviceList.nDeviceNum; i++) {
        if (!m_deviceList.pDeviceInfo[i]) continue;
        std::stringstream cameraName;
        cameraName << m_deviceList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.chUserDefinedName;
        QString camName = QString::fromStdString(cameraName.str());
        qDebug() << "Устройство" << i << "Имя:" << camName
                 << "Серийный номер:" << m_deviceList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.chSerialNumber;

        if (m_cameraNames.contains(camName)) {
            CameraFrameInfo* frameInfo = new CameraFrameInfo();
            CameraVideoFrameInfo* videoInfo = new CameraVideoFrameInfo();
            frameInfo->name = camName;
            frameInfo->id = i;
            videoInfo->name = camName;
            videoInfo->id = i;
            m_cameras.append(frameInfo);
            m_videoInfos.append(videoInfo);
            qDebug() << "Добавлена камера" << camName << "с ID" << i;
        } else {
            qDebug() << "Камера" << camName << "пропущена, так как отсутствует в m_cameraNames";
        }
    }

    if (m_cameras.isEmpty()) {
        QString errorMsg = QString("Не найдено ни одной камеры из списка: %1").arg(m_cameraNames.join(", "));
        qDebug() << errorMsg;
        emit errorOccurred("Camera", errorMsg);

        // Запуск таймера для повторной проверки через 30 секунд
        if (!m_checkCameraTimer->isActive()) {
            qDebug() << "Запуск таймера для повторной проверки камер через 30 секунд";
            m_checkCameraTimer->start(30000); // 30 секунд
        }
        return -1;
    }

    // Камеры найдены, вызов reconnectCameras()
    qDebug() << "Камеры найдены, вызов reconnectCameras()";
    reconnectCameras();

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
            ;
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
    for (CameraVideoFrameInfo* videoInfo : m_videoInfos) {
        videoInfo->handle = nullptr;
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
        if (m_reconnectAttempts < m_maxReconnectAttempts) {
            m_reconnectAttempts++;
            qDebug() << "Попытка переподключения" << m_reconnectAttempts << "из" << m_maxReconnectAttempts;
            reconnectCameras();
        } else {
            qDebug() << "Достигнуто максимальное количество попыток переподключения";
            stopAll();
            emit finished();
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
