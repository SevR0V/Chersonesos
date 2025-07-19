#include "logger.h"

QFile Logger::logFile;
QMutex Logger::logMutex;
QString Logger::logDirectory = "logs";
qint64 Logger::maxLogFileSize = 5 * 1024 * 1024; // 5 МБ по умолчанию
int Logger::maxLogFiles = 10; // Хранить до 10 лог-файлов

void Logger::installMessageHandler() {
    qInstallMessageHandler(messageHandler);
}

void Logger::setLogDirectory(const QString& directory) {
    logDirectory = directory;
}

void Logger::setMaxLogFileSize(qint64 size) {
    maxLogFileSize = size > 0 ? size : maxLogFileSize;
}

void Logger::setMaxLogFiles(int count) {
    maxLogFiles = count > 0 ? count : maxLogFiles;
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    QMutexLocker locker(&logMutex);

    // Открываем лог-файл, если он ещё не открыт
    if (!logFile.isOpen()) {
        logFile.setFileName(getLogFilePath());
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            // Если файл не открывается, выводим в stderr
            fprintf(stderr, "Не удалось открыть лог-файл: %s\n", qPrintable(logFile.errorString()));
            return;
        }
    }

    // Проверяем размер файла и выполняем ротацию при необходимости
    if (logFile.size() >= maxLogFileSize) {
        logFile.close();
        rotateLogs();
        logFile.setFileName(getLogFilePath());
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            fprintf(stderr, "Не удалось открыть лог-файл после ротации: %s\n", qPrintable(logFile.errorString()));
            return;
        }
    }

    QTextStream out(&logFile);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString messageType;
    switch (type) {
    case QtDebugMsg: messageType = "DEBUG"; break;
    case QtInfoMsg: messageType = "INFO"; break;
    case QtWarningMsg: messageType = "WARNING"; break;
    case QtCriticalMsg: messageType = "CRITICAL"; break;
    case QtFatalMsg: messageType = "FATAL"; break;
    }

    // Формат: [Время] [Тип] [Файл:Строка] Сообщение
    QString logMessage = QString("[%1] [%2] [%3:%4] %5\n")
                             .arg(timestamp)
                             .arg(messageType)
                             .arg(context.file ? QString(context.file) : "unknown")
                             .arg(context.line)
                             .arg(msg);

    out << logMessage;
    out.flush();

    // Также выводим в консоль для отладки
    fprintf(stderr, "%s", qPrintable(logMessage));

    if (type == QtFatalMsg) {
        logFile.close();
        abort();
    }
}

void Logger::rotateLogs() {
    std::filesystem::path logDir = logDirectory.toStdString();
    if (!std::filesystem::exists(logDir)) {
        std::filesystem::create_directory(logDir);
    }

    // Получаем список существующих лог-файлов
    std::vector<std::filesystem::path> logFiles;
    for (const auto& entry : std::filesystem::directory_iterator(logDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".log") {
            logFiles.push_back(entry.path());
        }
    }

    // Сортируем файлы по времени изменения (от старых к новым)
    std::sort(logFiles.begin(), logFiles.end(), [](const auto& a, const auto& b) {
        return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
    });

    // Удаляем старые файлы, если их больше maxLogFiles
    while (logFiles.size() >= static_cast<size_t>(maxLogFiles)) {
        try {
            std::filesystem::remove(logFiles.front());
            logFiles.erase(logFiles.begin());
        } catch (const std::filesystem::filesystem_error& e) {
            fprintf(stderr, "Ошибка удаления старого лог-файла: %s\n", e.what());
        }
    }
}

QString Logger::getLogFilePath() {
    std::filesystem::path logDir = logDirectory.toStdString();
    if (!std::filesystem::exists(logDir)) {
        std::filesystem::create_directory(logDir);
    }

    auto now = std::time(nullptr);
    std::tm timeInfo;
#ifdef _MSC_VER
    localtime_s(&timeInfo, &now);
#else
    timeInfo = *std::localtime(&now);
#endif
    std::stringstream ss;
    ss << std::put_time(&timeInfo, "chersonesos_%Y%m%d_%H%M%S.log");
    return QString::fromStdString((logDir / ss.str()).string());
}

