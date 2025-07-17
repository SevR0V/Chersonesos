#include "video_recorder.h"


VideoRecorder::VideoRecorder(RecordFrameInfo* recordInfo, QObject* parent)
    : QObject(parent), m_recordInfo(recordInfo), m_isRecording(false), m_recordInterval(30), m_storedVideoFilesLimit(10) {}

void VideoRecorder::setRecordInterval(int interval) {
    m_recordInterval = interval > 0 ? interval : 30;
    qDebug() << "Установлен интервал записи для камеры" << m_recordInfo->name << ": " << m_recordInterval << "секунд";
}

void VideoRecorder::setStoredVideoFilesLimit(int limit) {
    m_storedVideoFilesLimit = limit;
}

void VideoRecorder::manageStoredFiles() {
    if (m_storedVideoFilesLimit == 0) {
        QString errorMsg = QString("Старые записи не удаляются: проверяйте свободное место для камеры %1").arg(m_recordInfo->name);
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
                               .arg(m_recordInfo->name).arg(e.what());
        qDebug() << errorMsg;
        emit errorOccurred("VideoRecorder", errorMsg);
    }
}

void VideoRecorder::startRecording() {
    if (m_isRecording) {
        QString errorMsg = QString("Запись уже активна для камеры %1").arg(m_recordInfo->name);
        qDebug() << errorMsg;
        emit errorOccurred("VideoRecorder", errorMsg);
        return;
    }

    m_isRecording = true;
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

    std::string sessionDirName = generateSessionDirectoryName();
    m_sessionDirectory = pathToVideoDirectory / sessionDirName;
    try {
        if (!std::filesystem::exists(m_sessionDirectory)) {
            if (!std::filesystem::create_directory(m_sessionDirectory)) {
                QString errorMsg = QString("Не удалось создать сессионную директорию %1 для камеры %2")
                                       .arg(QString::fromStdString(m_sessionDirectory.string())).arg(m_recordInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                if (videoWriter.isOpened()) videoWriter.release();
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        } else {
            if (!std::filesystem::is_directory(m_sessionDirectory)) {
                QString errorMsg = QString("Путь %1 существует, но не является директорией для камеры %2")
                                       .arg(QString::fromStdString(m_sessionDirectory.string())).arg(m_recordInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                if (videoWriter.isOpened()) videoWriter.release();
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
            std::filesystem::perms perms = std::filesystem::status(m_sessionDirectory).permissions();
            if ((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none) {
                QString errorMsg = QString("Нет прав на запись в сессионную директорию %1 для камеры %2")
                                       .arg(QString::fromStdString(m_sessionDirectory.string())).arg(m_recordInfo->name);
                qDebug() << errorMsg;
                m_isRecording = false;
                if (videoWriter.isOpened()) videoWriter.release();
                emit errorOccurred("VideoRecorder", errorMsg);
                emit recordingFailed(errorMsg);
                return;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        QString errorMsg = QString("Ошибка файловой системы при создании сессионной директории для камеры %1: %2")
                               .arg(m_recordInfo->name).arg(e.what());
        m_isRecording = false;
        if (videoWriter.isOpened()) videoWriter.release();
        emit errorOccurred("VideoRecorder", errorMsg);
        emit recordingFailed(errorMsg);
        return;
    }

    startNewSegment();
}

void VideoRecorder::stopRecording() {
    m_isRecording = false;
    if (videoWriter.isOpened()) {
        videoWriter.release();
        qDebug() << "Запись видео остановлена для камеры" << m_recordInfo->name;
    }
}

void VideoRecorder::recordFrame() {
    if (!m_isRecording) return;

    if (!m_timer.isValid()) {
        m_timer.start();
    }

    if (m_timer.elapsed() >= m_recordInterval * 1000) {
        if (videoWriter.isOpened()) {
            videoWriter.release();
            qDebug() << "Запись сегмента видео завершена для файла:" << QString::fromStdString(fileName) << "через" << m_timer.elapsed() << "мс";
            emit recordingFinished();
        }
        manageStoredFiles();
        startNewSegment();
        m_timer.restart();
        qDebug() << "Новый сегмент начат для камеры" << m_recordInfo->name;
    }

    cv::Mat frame;
    {
        QMutexLocker locker(m_recordInfo->mutex);
        if (!m_recordInfo->img.empty()) {
            frame = m_recordInfo->img.clone();
        }
    }

    if (!frame.empty() && videoWriter.isOpened()) {
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        try {
            videoWriter.write(frame);
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
}

void VideoRecorder::startNewSegment() {
    fileName = generateFileName("chersonesos", ".avi");
    std::string filePath = (m_sessionDirectory / fileName).string();

    int fourccCode = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    cv::Size videoResolution;
    int realFPS = 20;

    {
        QMutexLocker locker(m_recordInfo->mutex);
        if (m_recordInfo->img.empty()) {
            QString errorMsg = QString("Кадр пуст для камеры %1").arg(m_recordInfo->name);
            qDebug() << errorMsg;
            if (videoWriter.isOpened()) videoWriter.release();
            emit errorOccurred("VideoRecorder", errorMsg);
            return;
        }
        videoResolution = m_recordInfo->img.size();
    }

    videoWriter.open(filePath, fourccCode, realFPS, videoResolution);
    if (!videoWriter.isOpened()) {
        QString errorMsg = QString("Не удалось открыть VideoWriter для файла %1, Кодек: %2, FPS: %3, Разрешение: %4x%5")
                               .arg(QString::fromStdString(filePath)).arg(fourccCode).arg(realFPS)
                               .arg(videoResolution.width).arg(videoResolution.height);
        qDebug() << errorMsg;
        if (videoWriter.isOpened()) videoWriter.release();
        emit errorOccurred("VideoRecorder", errorMsg);
        return;
    }

    qDebug() << "Запись видео начата для файла:" << QString::fromStdString(fileName)
             << ", FPS:" << realFPS << ", Разрешение:" << videoResolution.width << "x" << videoResolution.height;
    emit recordingStarted();
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

std::string VideoRecorder::generateSessionDirectoryName() {
    auto now = std::time(nullptr);
    std::tm timeInfo;
    std::stringstream ss;
#ifdef _MSC_VER
    localtime_s(&timeInfo, &now);
#else
    timeInfo = *std::localtime(&now);
#endif
    std::string cameraName = sanitizeFileName(m_recordInfo->name.toStdString());
    ss << cameraName << "_" << std::put_time(&timeInfo, "%Y%m%d_%H%M%S");
    return ss.str();
}
