#ifndef LOGGER_H
#define LOGGER_H

#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QDebug>

class Logger {
public:
    static void installMessageHandler();
    static void setLogDirectory(const QString& directory);
    static void setMaxLogFileSize(qint64 size); // В байтах
    static void setMaxLogFiles(int count);

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);
    static void rotateLogs();
    static QString getLogFilePath();
    static QFile logFile;
    static QMutex logMutex;
    static QString logDirectory;
    static qint64 maxLogFileSize;
    static int maxLogFiles;
};

#endif // LOGGER_H
