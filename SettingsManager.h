#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class SettingsManager : public QObject
{
    Q_OBJECT

public:
    // Удаляем конструкторы копирования и присваивания
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    // Основные методы
    bool initialize(const QString& filePath = "settings.json");
    QJsonValue get(const QString& key, const QJsonValue& defaultValue = QJsonValue()) const;

    // Статический метод для доступа к единственному экземпляру
    static SettingsManager& instance();

    // Методы для работы с настройками
    bool loadFromFile(const QString& filePath);
    bool saveToFile(const QString& filePath) const;

    // Методы доступа к настройкам
    QString getString(const QString& key, const QString& defaultValue = "") const;
    int getInt(const QString& key, int defaultValue = 0) const;
    bool getBool(const QString& key, bool defaultValue = false) const;
    double getDouble(const QString& key, double defaultValue = 0.0) const;
    QJsonObject getObject(const QString& key) const;

    // Методы для установки значений
    void setValue(const QString& key, const QJsonValue& value);
    void setString(const QString& key, const QString& value);
    void setInt(const QString& key, int value);
    void setBool(const QString& key, bool value);
    void setDouble(const QString& key, double value);
    void setObject(const QString& key, const QJsonObject& value);

signals:
    void settingsChanged();

private:
    // Приватный конструктор
    explicit SettingsManager(QObject* parent = nullptr);
    //Q_DISABLE_COPY(SettingsManager);

    bool m_initialized = false;
    QJsonObject m_settings; // Основной объект для хранения настроек

};

#endif // SETTINGSMANAGER_H
