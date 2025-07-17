#include "mainwindow.h"
#include <QApplication>
#include "logger.h"
#include "settingsmanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Настройка логирования
    Logger::setLogDirectory("logs");
    Logger::setMaxLogFileSize(5 * 1024 * 1024); // 5 MB
    Logger::setMaxLogFiles(10);
    Logger::installMessageHandler();
    // Инициализируем менеджер настроек один раз при запуске
    if(!SettingsManager::instance().initialize()) {
        qCritical() << "Failed to load settings!";
        return 1;
    }
    MainWindow w;
    w.show();

    return a.exec();
}
