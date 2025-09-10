#include "video_recorder.h"

VideoRecorder::VideoRecorder(RecordFrameInfo* recordInfo, OverlayFrameInfo* overlayInfo, QObject* parent)
    : QObject(parent), m_recordInfo(recordInfo), m_overlayInfo(overlayInfo), m_isRecording(false), m_recordInterval(30), m_storedVideoFilesLimit(10), m_frameCount(0), m_realFPS(20), m_recordMode(), m_videoResolution(cv::Size(1440, 1080)) {}

void VideoRecorder::setRecordInterval(int interval) {
    m_recordInterval = interval > 0 ? interval : 30;
    qDebug() << "Установлен интервал записи для камеры" << m_recordInfo->name << ": " << m_recordInterval << "секунд";
}

void VideoRecorder::setStoredVideoFilesLimit(int limit) {
    m_storedVideoFilesLimit = limit;
}

void VideoRecorder::setRecordMode(RecordMode mode) {
    m_recordMode = mode;
    qDebug() << "Установлен RecordMode для камеры" << m_recordInfo->name << ": " << static_cast<int>(mode);
}

void VideoRecorder::manageStoredFiles() {
    QThreadPool::globalInstance()->start([this]() {
            if (m_storedVideoFilesLimit == 0) {
                QString errorMsg = QString("Старые записи не удаляются: проверяйте свободное место для камеры %1").arg(m_recordInfo->name);
                qDebug() << errorMsg;
                emit errorOccurred("VideoRecorder", errorMsg);
                return;
            }

            try {
                std::vector<std::filesystem::directory_entry> videoFiles;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(m_sessionDirectory)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".avi") {
                        videoFiles.push_back(entry);
                    }
                }

                std::sort(videoFiles.begin(), videoFiles.end(),
                          [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
                              return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
                          });

                while (videoFiles.size() > static_cast<size_t>(m_storedVideoFilesLimit)) {
                    try {
                        std::filesystem::remove(videoFiles.front().path());
                        qDebug() << "Удален старый видеофайл:" << QString::fromStdString(videoFiles.front().path().string());
                        videoFiles.erase(videoFiles.begin());
                    } catch (const std::filesystem::filesystem_error& e) {
                        QString errorMsg = QString("Ошибка при удалении старого видеофайла %1: %2")
                                               .arg(QString::fromStdString(videoFiles.front().path().string()))
                                               .arg(e.what());
                        qDebug() << errorMsg;
                        emit errorOccurred("VideoRecorder", errorMsg);
                    }
                }
            } catch (const std::filesystem::filesystem_error& e) {
                QString errorMsg = QString("Ошибка при управлении видеофайлами для камеры %1: %2")
                                       .arg(m_recordInfo->name).arg(e.what());
                qDebug() << errorMsg;
                emit errorOccurred("VideoRecorder", errorMsg);
            }
    });
}

void VideoRecorder::startRecording() {

    if (m_recordMode == WithOverlay || m_recordMode == Both) {
        if (!m_overlayInfo) {
            QString errorMsg = "OverlayInfo не предоставлен для режима с оверлеем";
            qDebug() << errorMsg;
            emit errorOccurred("VideoRecorder", errorMsg);
            m_isRecording = false;
            return;
        }
    }

    if (m_isRecording) {
        QString errorMsg = QString("Запись уже активна для камеры %1").arg(m_recordInfo->name);
        qDebug() << errorMsg;
        emit errorOccurred("VideoRecorder", errorMsg);
        return;
    }

    if (videoWriter.isOpened()) {
        videoWriter.release();
        qDebug() << "Предыдущая запись принудительно завершена для камеры" << m_recordInfo->name;
    }
    if (videoWriterOverlay.isOpened()) {
        videoWriterOverlay.release();
    }

    m_isRecording = true;
    m_frameCount = 0;
    m_timer.invalidate();
    m_timer.start();
    qDebug() << "Начало записи видео для камеры" << m_recordInfo->name;

    std::filesystem::path pathToVideoDirectory = std::filesystem::current_path() / "video";
    try {
        if (!std::filesystem::exists(pathToVideoDirectory)) {
            if (!std::filesystem::create_directory(pathToVideoDirectory)) {
                QString errorMsg = QString("Не удалось создать директорию video для камеры %1").arg(m_recordInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        } else {
            std::filesystem::perms perms = std::filesystem::status(pathToVideoDirectory).permissions();
            if ((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none) {
                QString errorMsg = QString("Нет прав на запись в директорию video для камеры %1").arg(m_recordInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        QString errorMsg = QString("Ошибка файловой системы для камеры %1: %2")
                               .arg(m_recordInfo->name).arg(e.what());
        m_isRecording = false;
        emit errorOccurred("VideoRecorder", errorMsg);
        emit recordingFailed(errorMsg);
        return;
    }

    std::string dateDirName = generateDateDirectoryName();
    std::filesystem::path dateDirectory = pathToVideoDirectory / dateDirName;
    try {
        if (!std::filesystem::exists(dateDirectory)) {
            if (!std::filesystem::create_directory(dateDirectory)) {
                QString errorMsg = QString("Не удалось создать директорию даты %1 для камеры %2")
                                       .arg(QString::fromStdString(dateDirectory.string())).arg(m_recordInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        } else {
            std::filesystem::perms perms = std::filesystem::status(dateDirectory).permissions();
            if ((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none) {
                QString errorMsg = QString("Нет прав на запись в директорию даты %1 для камеры %2")
                                       .arg(QString::fromStdString(dateDirectory.string())).arg(m_recordInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        QString errorMsg = QString("Ошибка файловой системы для камеры %1: %2")
                               .arg(m_recordInfo->name).arg(e.what());
        m_isRecording = false;
        emit errorOccurred("VideoRecorder", errorMsg);
        emit recordingFailed(errorMsg);
        return;
    }

    std::string timeDirName = generateTimeDirectoryName();
    m_sessionDirectory = dateDirectory / timeDirName;

    //cachedFiles.clear();

    try {
        if (!std::filesystem::exists(m_sessionDirectory)) {
            if (!std::filesystem::create_directory(m_sessionDirectory)) {
                QString errorMsg = QString("Не удалось создать сессионную директорию %1 для камеры %2")
                                       .arg(QString::fromStdString(m_sessionDirectory.string())).arg(m_recordInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        } else {
            std::filesystem::perms perms = std::filesystem::status(m_sessionDirectory).permissions();
            if ((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none) {
                QString errorMsg = QString("Нет прав на запись в сессионную директорию %1 для камеры %2")
                                       .arg(QString::fromStdString(m_sessionDirectory.string())).arg(m_recordInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        QString errorMsg = QString("Ошибка файловой системы для камеры %1: %2")
                               .arg(m_recordInfo->name).arg(e.what());
        m_isRecording = false;
        emit errorOccurred("VideoRecorder", errorMsg);
        emit recordingFailed(errorMsg);
        return;
    }

    startNewSegment();
}

void VideoRecorder::stopRecording() {
    if (!m_isRecording) {
        return;
    }

    m_isRecording = false;

    if (videoWriter.isOpened()) {
        videoWriter.release();
        qDebug() << "Запись видео завершена для камеры" << m_recordInfo->name;
        emit recordingFinished();


    }

    if ((m_recordMode == WithOverlay || m_recordMode == Both) && videoWriterOverlay.isOpened()) {
        videoWriterOverlay.release();
        if (m_recordMode == Both) {
            std::filesystem::path newFilePathOverlay = m_sessionDirectory / fileNameOverlay;

        }
    }
    manageStoredFiles();
}

void VideoRecorder::recordFrame() {
    if (!m_isRecording) {
        return;
    }

    cv::Mat frame;
    bool hasFrame = false;

    if (m_recordMode == WithOverlay || m_recordMode == Both) {
        QMutexLocker locker(m_overlayInfo->mutex);
        if (!m_overlayInfo->overlayQueue.empty()) {
            frame = m_overlayInfo->overlayQueue.front();
            m_overlayInfo->overlayQueue.pop_front();
            hasFrame = true;
        }
    } else {
        QMutexLocker locker(m_recordInfo->mutex);
        if (!m_recordInfo->frameQueue.empty()) {
            frame = m_recordInfo->frameQueue.front();
            m_recordInfo->frameQueue.pop_front();
            hasFrame = true;
        }
    }

    if (hasFrame) {
        // Инициализируем разрешение, если еще не
        if (m_videoResolution == cv::Size(0, 0)) {
            m_videoResolution = frame.size();
        }

        try {
            if (m_recordMode == Both) {
                // Запись original в videoWriter
                cv::Mat originalFrame;
                bool hasOriginal = false;
                {
                    QMutexLocker locker(m_recordInfo->mutex);
                    if (!m_recordInfo->frameQueue.empty()) {
                        originalFrame = m_recordInfo->frameQueue.front();
                        m_recordInfo->frameQueue.pop_front();
                        hasOriginal = true;
                    }
                }
                if (hasOriginal && videoWriter.isOpened()) {
                    videoWriter.write(originalFrame);
                }

                // Запись overlay в videoWriterOverlay
                if (videoWriterOverlay.isOpened()) {
                    videoWriterOverlay.write(frame);
                }
            } else {
                if (videoWriter.isOpened()) {
                    videoWriter.write(frame);
                }
            }
            m_frameCount++;
            //qDebug() << "Записан кадр #" << m_frameCount << "для файла" << QString::fromStdString(fileName) << ", время:" << m_timer.elapsed() / 1000.0 << "секунд";
        } catch (const cv::Exception& e) {
            QString errorMsg = QString("Ошибка записи кадра для камеры %1: %2")
                                   .arg(m_recordInfo->name).arg(e.what());
            qDebug() << errorMsg;
            if (videoWriter.isOpened()) videoWriter.release();
            if (videoWriterOverlay.isOpened()) videoWriterOverlay.release();
            emit errorOccurred("VideoRecorder", errorMsg);
        }
    } else {
        //qDebug() << "Пропущен пустой кадр или VideoWriter не открыт для камеры" << m_recordInfo->name;
        //return;
    }

    int maxFrames = m_recordInterval * m_realFPS;
    if (m_frameCount >= maxFrames || m_timer.elapsed() >= m_recordInterval * 1500) {
        if (videoWriter.isOpened()) {
            videoWriter.release();
        }
        if (videoWriterOverlay.isOpened()) {
            videoWriterOverlay.release();
        }
        qDebug() << "Запись сегмента видео завершена для файла:" << QString::fromStdString(fileName)
                 << ", кадров:" << m_frameCount << ", время:" << m_timer.elapsed() / 1000.0 << "секунд";
        emit recordingFinished();

        std::filesystem::path newFilePath = m_sessionDirectory / fileName;

        if (m_recordMode == Both) {
            std::filesystem::path newFilePathOverlay = m_sessionDirectory / fileNameOverlay;

        }


        startNewSegment();
        manageStoredFiles();
        m_frameCount = 0;
        m_timer.invalidate();
        m_timer.start();
        qDebug() << "Новый сегмент начат для камеры" << m_recordInfo->name;
    }
}

void VideoRecorder::startNewSegment() {
    fileName = generateFileName("chersonesos", ".avi");
    std::string filePath = (m_sessionDirectory / fileName).string();

    int fourccCode = cv::VideoWriter::fourcc('X', 'V', 'I', 'D');

    videoWriter.open(filePath, fourccCode, m_realFPS, m_videoResolution);
    if (!videoWriter.isOpened()) {
        QString errorMsg = QString("Не удалось открыть VideoWriter для файла %1, Кодек: %2, FPS: %3, Разрешение: %4x%5")
                               .arg(QString::fromStdString(filePath)).arg(fourccCode).arg(m_realFPS)
                               .arg(m_videoResolution.width).arg(m_videoResolution.height);
        qDebug() << errorMsg;
        if (videoWriter.isOpened()) videoWriter.release();
        emit errorOccurred("VideoRecorder", errorMsg);
        return;
    }

    qDebug() << "Запись видео начата для файла:" << QString::fromStdString(fileName)
             << ", FPS:" << m_realFPS << ", Разрешение:" << m_videoResolution.width << "x" << m_videoResolution.height;
    emit recordingStarted();

    if (m_recordMode == Both) {
        fileNameOverlay = generateFileName("chersonesos_overlay", ".avi");
        std::string filePathOverlay = (m_sessionDirectory / fileNameOverlay).string();
        videoWriterOverlay.open(filePathOverlay, fourccCode, m_realFPS, m_videoResolution);
        if (!videoWriterOverlay.isOpened()) {
            QString errorMsg = QString("Не удалось открыть VideoWriter для оверлея %1").arg(QString::fromStdString(filePathOverlay));
            qDebug() << errorMsg;
            emit errorOccurred("VideoRecorder", errorMsg);
            videoWriter.release(); // Закрываем основной, если оверлей не открылся
            return;
        }
    } else if (m_recordMode == WithOverlay) {
        // Для WithOverlay используем только videoWriter для оверлея
    }


}

std::string VideoRecorder::sanitizeFileName(const std::string& input) {
    std::string sanitized = input;
    for (char& c : sanitized) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || c == ' ') {
            c = '_';
        }
    }
    while (!sanitized.empty() && sanitized.front() == '_') {
        sanitized.erase(0, 1);
    }
    while (!sanitized.empty() && sanitized.back() == '_') {
        sanitized.pop_back();
    }
    if (sanitized.length() > 200) { // Увеличили лимит
        sanitized = sanitized.substr(0, 200);
    }
    return sanitized.empty() ? "unknown_camera" : sanitized;
}

std::string VideoRecorder::generateFileName(const std::string& prefix, const std::string& extension) {
    auto now = std::time(nullptr);
    std::tm timeInfo;
    std::stringstream ss;
#ifdef _MSC_VER
    localtime_s(&timeInfo, &now);
#else
    timeInfo = *std::localtime(&now);
#endif
    std::string cameraName = sanitizeFileName(m_recordInfo->name.toStdString());
    ss << prefix << "_" << cameraName << "_" << std::put_time(&timeInfo, "%Y%m%d_%H%M%S") << extension;
    return ss.str();
}

std::string VideoRecorder::generateDateDirectoryName() {
    auto now = std::time(nullptr);
    std::tm timeInfo;
    std::stringstream ss;
#ifdef _MSC_VER
    localtime_s(&timeInfo, &now);
#else
    timeInfo = *std::localtime(&now);
#endif
    ss << std::put_time(&timeInfo, "%d%m%Y");
    return ss.str();
}

std::string VideoRecorder::generateTimeDirectoryName() {
    auto now = std::time(nullptr);
    std::tm timeInfo;
    std::stringstream ss;
#ifdef _MSC_VER
    localtime_s(&timeInfo, &now);
#else
    timeInfo = *std::localtime(&now);
#endif
    std::string cameraName = sanitizeFileName(m_recordInfo->name.toStdString());
    ss << cameraName << "_" << std::put_time(&timeInfo, "%H%M%S");
    return ss.str();
}
