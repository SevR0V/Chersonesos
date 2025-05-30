#include "video_recorder.h"

VideoRecorder::VideoRecorder(CameraVideoFrameInfo* videoInfo, QObject* parent)
    : QObject(parent), m_videoInfo(videoInfo), m_isRecording(false), m_recordInterval(30), m_storedVideoFilesLimit(10) {}

void VideoRecorder::setRecordInterval(int interval) {
    m_recordInterval = interval > 0 ? interval : 30;
}

void VideoRecorder::setStoredVideoFilesLimit(int limit) {
    m_storedVideoFilesLimit = limit;
}

void VideoRecorder::manageStoredFiles() {
    if (m_storedVideoFilesLimit == 0) {
        QString errorMsg = QString("Старые записи не удаляются: проверяйте свободное место для камеры %1").arg(m_videoInfo->name);
        qDebug() << errorMsg;
        emit errorOccurred("VideoRecorder", errorMsg);
        return;
    }

    try {
        std::vector<std::filesystem::directory_entry> videoFiles;
        for (const auto& entry : std::filesystem::directory_iterator(m_sessionDirectory)) {
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
                               .arg(m_videoInfo->name).arg(e.what());
        qDebug() << errorMsg;
        emit errorOccurred("VideoRecorder", errorMsg);
    }
}

void VideoRecorder::startRecording() {
    if (m_isRecording) {
        QString errorMsg = QString("Запись уже активна для камеры %1").arg(m_videoInfo->name);
        qDebug() << errorMsg;
        emit errorOccurred("VideoRecorder", errorMsg);
        return;
    }

    m_isRecording = true;
    qDebug() << "Начало записи видео для камеры" << m_videoInfo->name;

    // Проверяем и создаем папку video
    std::filesystem::path pathToVideoDirectory = std::filesystem::current_path() / "video";
    try {
        if (!std::filesystem::exists(pathToVideoDirectory)) {
            if (!std::filesystem::create_directory(pathToVideoDirectory)) {
                QString errorMsg = QString("Не удалось создать директорию video для камеры %1").arg(m_videoInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        } else {
            std::filesystem::perms perms = std::filesystem::status(pathToVideoDirectory).permissions();
            if ((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none) {
                QString errorMsg = QString("Нет прав на запись в директорию video для камеры %1").arg(m_videoInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        QString errorMsg = QString("Ошибка файловой системы для камеры %1: %2")
                               .arg(m_videoInfo->name).arg(e.what());
        m_isRecording = false;
        emit errorOccurred("VideoRecorder", errorMsg);
        emit recordingFailed(errorMsg);
        return;
    }

    // Создаем сессионную директорию
    std::string sessionDirName = generateSessionDirectoryName();
    m_sessionDirectory = pathToVideoDirectory / sessionDirName;
    try {
        if (!std::filesystem::exists(m_sessionDirectory)) {
            if (!std::filesystem::create_directory(m_sessionDirectory)) {
                QString errorMsg = QString("Не удалось создать сессионную директорию %1 для камеры %2")
                                       .arg(QString::fromStdString(m_sessionDirectory.string())).arg(m_videoInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        } else {
            if (!std::filesystem::is_directory(m_sessionDirectory)) {
                QString errorMsg = QString("Путь %1 существует, но не является директорией для камеры %2")
                                       .arg(QString::fromStdString(m_sessionDirectory.string())).arg(m_videoInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
            std::filesystem::perms perms = std::filesystem::status(m_sessionDirectory).permissions();
            if ((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none) {
                QString errorMsg = QString("Нет прав на запись в сессионную директорию %1 для камеры %2")
                                       .arg(QString::fromStdString(m_sessionDirectory.string())).arg(m_videoInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        QString errorMsg = QString("Ошибка файловой системы при создании сессионной директории для камеры %1: %2")
                               .arg(m_videoInfo->name).arg(e.what());
        m_isRecording = false;
        emit errorOccurred("VideoRecorder", errorMsg);
        emit recordingFailed(errorMsg);
        return;
    }

    // Инициализируем параметры видео
    int fourccCode = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    cv::Size videoResolution;
    int realFPS = 30;

    {
        QMutexLocker locker(m_videoInfo->mutex);
        if (m_videoInfo->frameBuffer.empty()) {
            QString errorMsg = QString("Буфер кадров пуст для камеры %1").arg(m_videoInfo->name);
            qDebug() << errorMsg;
            m_isRecording = false;
            emit errorOccurred("VideoRecorder", errorMsg);
            emit recordingFailed(errorMsg);
            return;
        }
        bool validFrameFound = false;

        for (const auto& frame : m_videoInfo->frameBuffer) {
            if (!frame.empty()) {
                videoResolution = frame.size();
                validFrameFound = true;
                break;
            }
        }

        if (!validFrameFound) {
            QString errorMsg = QString("Все кадры в буфере пусты для камеры %1").arg(m_videoInfo->name);
            qDebug() << errorMsg;
            m_isRecording = false;
            emit errorOccurred("VideoRecorder", errorMsg);
            emit recordingFailed(errorMsg);
            return;
        }
    }

    QElapsedTimer timer;
    timer.start();

    while (m_isRecording) {
        // Генерируем имя файла и путь внутри сессионной директории
        std::string fileName = generateFileName("chersonesos", ".avi");
        std::string filePath = (m_sessionDirectory / fileName).string();

        // Инициализируем VideoWriter
        cv::VideoWriter videoWriter(filePath, fourccCode, realFPS, videoResolution);
        if (!videoWriter.isOpened()) {
            QString errorMsg = QString("Не удалось открыть VideoWriter для файла %1, Кодек: %2, FPS: %3, Разрешение: %4x%5")
                                   .arg(QString::fromStdString(filePath)).arg(fourccCode).arg(realFPS)
                                   .arg(videoResolution.width).arg(videoResolution.height);
            qDebug() << errorMsg;
            m_isRecording = false;
            emit errorOccurred("VideoRecorder", errorMsg);
            emit recordingFailed(errorMsg);
            return;
        }

        qDebug() << "Запись видео начата для файла:" << QString::fromStdString(fileName)
                 << ", FPS:" << realFPS << ", Разрешение:" << videoResolution.width << "x" << videoResolution.height;
        emit recordingStarted();

        // Записываем кадры для текущего сегмента
        timer.restart();
        while (m_isRecording && timer.elapsed() < (m_recordInterval * 1000)) {
            cv::Mat frame;
            {
                QMutexLocker locker(m_videoInfo->mutex);
                int latestIndex = (m_videoInfo->bufferIndex == 0) ? 4 : m_videoInfo->bufferIndex - 1;
                if (!m_videoInfo->frameBuffer.empty() && !m_videoInfo->frameBuffer[latestIndex].empty()) {
                    frame = m_videoInfo->frameBuffer[latestIndex].clone();
                }
            }

            if (!frame.empty()) {
                try {
                    videoWriter.write(frame);
                } catch (const cv::Exception& e) {
                    QString errorMsg = QString("Ошибка записи кадра для камеры %1: %2")
                                           .arg(m_videoInfo->name).arg(e.what());
                    qDebug() << errorMsg;
                    videoWriter.release();
                    m_isRecording = false;
                    emit errorOccurred("VideoRecorder", errorMsg);
                    emit recordingFailed(errorMsg);
                    return;
                }
            } else {
                qDebug() << "Пропущен пустой кадр для камеры" << m_videoInfo->name;
            }

            QThread::usleep(1000);
        }

        videoWriter.release();
        qDebug() << "Запись сегмента видео завершена для файла:" << QString::fromStdString(fileName);
        emit recordingFinished();

        manageStoredFiles();
    }

    qDebug() << "Запись видео полностью завершена для камеры" << m_videoInfo->name;
}

void VideoRecorder::stopRecording() {
    m_isRecording = false;
    qDebug() << "Остановка записи видео для камеры" << m_videoInfo->name;
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
    std::string cameraName = sanitizeFileName(m_videoInfo->name.toStdString());
    ss << prefix << "_" << cameraName << "_" << std::put_time(&timeInfo, "%Y%m%d_%H%M%S") << extension;
    return ss.str();
}

std::string VideoRecorder::generateSessionDirectoryName() {
    auto now = std::time(nullptr);
    std::tm timeInfo;
    std::stringstream ss;
#ifdef _MSC_VER
    localtime_s(&timeInfo, &now);
#else
    timeInfo = *std::localtime(&now);
#endif
    std::string cameraName = sanitizeFileName(m_videoInfo->name.toStdString());
    ss << cameraName << "_" << std::put_time(&timeInfo, "%Y%m%d_%H%M%S");
    return ss.str();
}
