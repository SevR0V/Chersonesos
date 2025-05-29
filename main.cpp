#include "mainwindow.h"
#include <QApplication>
#include "logger.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Настройка логирования
    Logger::setLogDirectory("logs");
    Logger::setMaxLogFileSize(5 * 1024 * 1024); // 5 MB
    Logger::setMaxLogFiles(10);
    Logger::installMessageHandler();

    MainWindow w;
    w.show();

    return a.exec();
}
