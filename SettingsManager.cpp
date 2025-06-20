#include "SettingsManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>

SettingsManager::SettingsManager(QObject* parent)
    : QObject(parent)
{
}

SettingsManager& SettingsManager::instance()
{
    static SettingsManager instance;
    return instance;
}
bool SettingsManager::initialize(const QString& profileName)
{
    if(m_initialized) return true;

    QString baseDir = QCoreApplication::applicationDirPath();
    QString targetDirPath = baseDir + QDir::separator() + "Control Profiles";

    QDir dir;
    if (!dir.exists(targetDirPath)) {
        if (!dir.mkpath(targetDirPath)) {
            qWarning() << "Не удалось создать папку Control Profiles";
            return false;
        }
    }

    QString targetFile = targetDirPath + QDir::separator() + profileName;

    QFile file(targetFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open settings file:" << targetFile;
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if(error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
        return false;
    }

    m_settings = doc.object();
    m_initialized = true;
    return true;
}

QJsonValue SettingsManager::get(const QString& key, const QJsonValue& defaultValue) const
{
    if(!m_initialized) {
        qWarning() << "SettingsManager not initialized!";
        return defaultValue;
    }

    // Вариант 1: Использование find()
    auto it = m_settings.find(key);
    if (it != m_settings.end()) {
        return *it;
    }
    return defaultValue;

    /*
    // Вариант 2: Использование operator[]
    if (m_settings.contains(key)) {
        return m_settings[key];
    }
    return defaultValue;
    */
}
// не используется
bool SettingsManager::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open settings file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse settings file:" << error.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "Settings file does not contain a JSON object";
        return false;
    }

    m_settings = doc.object();
    emit settingsChanged();
    return true;
}

bool SettingsManager::saveToFile(const QString& profileName) const
{

    QString baseDir = QCoreApplication::applicationDirPath();
    QString targetDirPath = baseDir + QDir::separator() + "Control Profiles";

    QDir dir;
    if (!dir.exists(targetDirPath)) {
        if (!dir.mkpath(targetDirPath)) {
            qWarning() << "Не удалось создать папку Control Profiles";
            return false;
        }
    }

    QString targetFile = targetDirPath + QDir::separator() + profileName;

    QFile file(targetFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open settings file for writing:" << targetFile;
        return false;
    }

    QJsonDocument doc(m_settings);
    if (file.write(doc.toJson()) == -1) {
        qWarning() << "Failed to write settings to file";
        return false;
    }
    return true;
}

// Методы получения значений
QString SettingsManager::getString(const QString& key, const QString& defaultValue) const
{
    if (m_settings.contains(key) && m_settings[key].isString()) {
        return m_settings[key].toString();
    }
    return defaultValue;
}

int SettingsManager::getInt(const QString& key, int defaultValue) const
{
    if (m_settings.contains(key)) {
        if (m_settings[key].isDouble()) {
            return m_settings[key].toInt();
        } else if (m_settings[key].isString()) {
            bool ok;
            int value = m_settings[key].toString().toInt(&ok);
            if (ok) return value;
        }
    }
    return defaultValue;
}

bool SettingsManager::getBool(const QString& key, bool defaultValue) const
{
    if (m_settings.contains(key)) {
        if (m_settings[key].isBool()) {
            return m_settings[key].toBool();
        } else if (m_settings[key].isString()) {
            QString val = m_settings[key].toString().toLower();
            return (val == "true" || val == "1");
        }
    }
    return defaultValue;
}

double SettingsManager::getDouble(const QString& key, double defaultValue) const
{
    if (m_settings.contains(key) && m_settings[key].isDouble()) {
        return m_settings[key].toDouble();
    }
    return defaultValue;
}

QJsonObject SettingsManager::getObject(const QString& key) const
{
    if (m_settings.contains(key) && m_settings[key].isObject()) {
        return m_settings[key].toObject();
    }
    return QJsonObject();
}

// Методы установки значений
void SettingsManager::setValue(const QString& key, const QJsonValue& value)
{
    if (m_settings[key] != value) {
        m_settings[key] = value;
        emit settingsChanged();
    }
}

void SettingsManager::setString(const QString& key, const QString& value)
{
    setValue(key, value);
}

void SettingsManager::setInt(const QString& key, int value)
{
    setValue(key, value);
}

void SettingsManager::setBool(const QString& key, bool value)
{
    setValue(key, value);
}

void SettingsManager::setDouble(const QString& key, double value)
{
    setValue(key, value);
}

void SettingsManager::setObject(const QString& key, const QJsonObject& value)
{
    setValue(key, value);
}
