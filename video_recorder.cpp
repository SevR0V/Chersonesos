#include "video_recorder.h"

VideoRecorder::VideoRecorder(RecordFrameInfo* recordInfo, OverlayFrameInfo* overlayInfo, QObject* parent)
    : QObject(parent), m_recordInfo(recordInfo), m_overlayInfo(overlayInfo), m_isRecording(false), m_recordInterval(30), m_storedVideoFilesLimit(10), m_frameCount(0), m_realFPS(20), m_recordMode(WithOverlay) {}

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
    // Асинхронное сохранение в фоне
    QThreadPool::globalInstance()->start([=]() {
        if (m_storedVideoFilesLimit == 0) {
            QString errorMsg = QString("Старые записи не удаляются: проверяйте свободное место для камеры %1").arg(m_recordInfo->name);
            qDebug() << errorMsg;
            emit errorOccurred("VideoRecorder", errorMsg);
            return;
        }

        try {
            // Если кэш пустой — полный scan и заполнение
            if (cachedFiles.empty()) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(m_sessionDirectory)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".avi") {
                        cachedFiles.push_back(entry.path());
                    }
                }
                // Sort по last_write_time
                std::sort(cachedFiles.begin(), cachedFiles.end(),
                          [](const std::filesystem::path& a, const std::filesystem::path& b) {
                              return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
                          });
            }  // Иначе — кэш уже отсортирован (обновляется incrementally)

            // Удаляем excess из front (старые)
            while (cachedFiles.size() > static_cast<size_t>(m_storedVideoFilesLimit)) {
                try {
                    std::filesystem::remove(cachedFiles.front());
                    qDebug() << "Удален старый видеофайл:" << QString::fromStdString(cachedFiles.front().string());
                    cachedFiles.erase(cachedFiles.begin());
                } catch (const std::filesystem::filesystem_error& e) {
                    QString errorMsg = QString("Ошибка при удалении старого видеофайла %1: %2")
                                           .arg(QString::fromStdString(cachedFiles.front().string()))
                                           .arg(e.what());
                    qDebug() << errorMsg;
                    emit errorOccurred("VideoRecorder", errorMsg);
                    cachedFiles.erase(cachedFiles.begin());  // Удаляем из кэша даже если fs ошибка даже если fs ошибка
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            QString errorMsg = QString("Ошибка при управлении видеофайлами для камеры %1: %2")
                                   .arg(m_recordInfo->name).arg(e.what());
            qDebug() << errorMsg;
            emit errorOccurred("VideoRecorder", errorMsg);
            cachedFiles.clear();  // Сбрасываем кэш при ошибке
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


    if ((m_recordMode == WithOverlay || m_recordMode == Both) && videoWriterOverlay.isOpened()) {
        videoWriterOverlay.release();
        if (m_recordMode == Both) {
            std::filesystem::path newFilePathOverlay = m_sessionDirectory / fileNameOverlay;
            cachedFiles.push_back(newFilePathOverlay);  // Или отдельный кэш
            std::sort(cachedFiles.begin(), cachedFiles.end(),
                      [](const std::filesystem::path& a, const std::filesystem::path& b) {
                          return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
                      });
        }
    }
    if (videoWriter.isOpened()) {
        videoWriter.release();
        qDebug() << "Запись видео завершена для камеры" << m_recordInfo->name;
        emit recordingFinished();

        // + Обновление кэша: Добавляем последний файл
        std::filesystem::path newFilePath = m_sessionDirectory / fileName;
        cachedFiles.push_back(newFilePath);
        std::sort(cachedFiles.begin(), cachedFiles.end(),
                  [](const std::filesystem::path& a, const std::filesystem::path& b) {
                      return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
                  });
    }
    manageStoredFiles();
}
void VideoRecorder::recordFrame() {
    if (!m_isRecording) {
        return;
    }

    cv::Mat frame;
    {
        QMutexLocker locker(m_recordInfo->mutex);
        if (!m_recordInfo->frameQueue.empty()) {
            frame = m_recordInfo->frameQueue.front();  // + Берём front
            m_recordInfo->frameQueue.pop_front();      // + Удаляем
            //frame = frame.clone();                     // + Clone для модификации
        }
    }

    if (!frame.empty() && videoWriter.isOpened()) {
        try {


            cv::Mat overlay;
            if (m_recordMode == WithOverlay || m_recordMode == Both) {
                QMutexLocker overlayLocker(m_overlayInfo->mutex);
                if (!m_overlayInfo->overlayQueue.empty()) {
                    overlay = m_overlayInfo->overlayQueue.front();
                    m_overlayInfo->overlayQueue.pop_front();
                }
            }

            cv::Mat frameToWrite = frame.clone();
            if (!overlay.empty() && (m_recordMode == WithOverlay || m_recordMode == Both)) {
                if (overlay.size() == frame.size() && overlay.channels() == 4) {  // BGRA оверлей
                    cv::Mat overlayBGR;
                    cv::cvtColor(overlay, overlayBGR, cv::COLOR_BGRA2BGR);
                    for (int y = 0; y < frameToWrite.rows; ++y) {
                        for (int x = 0; x < frameToWrite.cols; ++x) {
                            uchar alpha = overlay.at<cv::Vec4b>(y, x)[3];
                            if (alpha > 0) {
                                frameToWrite.at<cv::Vec3b>(y, x) = (frameToWrite.at<cv::Vec3b>(y, x) * (255 - alpha) + overlayBGR.at<cv::Vec3b>(y, x) * alpha) / 255;
                            }
                        }
                    }
                }
            }

            // Запись (для NoOverlay или WithOverlay - в videoWriter, для Both - оригинал в videoWriter, с оверлеем в videoWriterOverlay
            if (videoWriter.isOpened()) {
                if (m_recordMode == Both) {
                    videoWriter.write(frame);  // Оригинал
                    if (videoWriterOverlay.isOpened()) {
                        videoWriterOverlay.write(frameToWrite);  // С оверлеем
                    }
                } else {
                    videoWriter.write(frameToWrite);  // С или без оверлея
                }
            }

            m_frameCount++;
            qDebug() << "Записан кадр #" << m_frameCount << "для файла" << QString::fromStdString(fileName)
                     << ", время:" << m_timer.elapsed() / 1000.0 << "секунд";
        } catch (const cv::Exception& e) {
            QString errorMsg = QString("Ошибка записи кадра для камеры %1: %2")
                                   .arg(m_recordInfo->name).arg(e.what());
            qDebug() << errorMsg;
            if (videoWriter.isOpened()) videoWriter.release();
            emit errorOccurred("VideoRecorder", errorMsg);
        }
    } else {
        qDebug() << "Пропущен пустой кадр или VideoWriter не открыт для камеры" << m_recordInfo->name;
    }

    int maxFrames = m_recordInterval * m_realFPS;
    if (m_frameCount >= maxFrames || m_timer.elapsed() >= m_recordInterval * 1500) {
        if (videoWriter.isOpened()) {
            videoWriter.release();
            qDebug() << "Запись сегмента видео завершена для файла:" << QString::fromStdString(fileName)
                     << ", кадров:" << m_frameCount << ", время:" << m_timer.elapsed() / 1000.0 << "секунд";
            emit recordingFinished();

            // + Обновление кэша: Добавляем новый файл
            std::filesystem::path newFilePath = m_sessionDirectory / fileName;
            cachedFiles.push_back(newFilePath);
            // Sort кэша (можно оптимизировать, но просто для small size)
            std::sort(cachedFiles.begin(), cachedFiles.end(),
                      [](const std::filesystem::path& a, const std::filesystem::path& b) {
                          return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
                      });
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
    cv::Size videoResolution;

    {
        QMutexLocker locker(m_recordInfo->mutex);
        if (m_recordInfo->frameQueue.empty()) {
            QString errorMsg = QString("Queue пустая для камеры %1").arg(m_recordInfo->name);
            qDebug() << errorMsg;
            if (videoWriter.isOpened()) videoWriter.release();
            emit errorOccurred("VideoRecorder", errorMsg);
            return;
        }
        videoResolution = m_recordInfo->frameQueue.front().size();  // + Берём size из front (без pop)
    }

    videoWriter.open(filePath, fourccCode, m_realFPS, videoResolution);
    if (!videoWriter.isOpened()) {
        QString errorMsg = QString("Не удалось открыть VideoWriter для файла %1, Кодек: %2, FPS: %3, Разрешение: %4x%5")
                               .arg(QString::fromStdString(filePath)).arg(fourccCode).arg(m_realFPS)
                               .arg(videoResolution.width).arg(videoResolution.height);
        qDebug() << errorMsg;
        if (videoWriter.isOpened()) videoWriter.release();
        emit errorOccurred("VideoRecorder", errorMsg);
        return;
    }

    qDebug() << "Запись видео начата для файла:" << QString::fromStdString(fileName)
             << ", FPS:" << m_realFPS << ", Разрешение:" << videoResolution.width << "x" << videoResolution.height;
    emit recordingStarted();

    if (m_recordMode == Both) {
        fileNameOverlay = generateFileName("chersonesos_overlay", ".avi");
        std::string filePathOverlay = (m_sessionDirectory / fileNameOverlay).string();
        videoWriterOverlay.open(filePathOverlay, fourccCode, m_realFPS, videoResolution);
        if (!videoWriterOverlay.isOpened()) {
            QString errorMsg = QString("Не удалось открыть VideoWriter для оверлея %1").arg(QString::fromStdString(filePathOverlay));
            qDebug() << errorMsg;
            emit errorOccurred("VideoRecorder", errorMsg);
        }
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
    if (sanitized.length() > 100) {
        sanitized = sanitized.substr(0, 100);
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
