#include "ProfileManager.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QDebug>

bool ProfileManager::load(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Не удалось открыть файл:" << filePath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        qWarning() << "Ошибка: файл не содержит корректный JSON объект";
        return false;
    }

    profileObject = doc.object();
    return true;
}

bool ProfileManager::loadFromProfileName(const QString& profileName) {
    if (profileName.isEmpty()) {
        qWarning() << "Имя профиля пустое — не могу загрузить.";
        return false;
    }

    QString baseDir = QCoreApplication::applicationDirPath();
    QString targetFile = baseDir + QDir::separator() +
                         "Control Profiles" + QDir::separator() +
                         profileName + ".json";

    QFile file(targetFile);
    if (!file.exists()) {
        qWarning() << "Файл профиля не найден:" << targetFile;
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Не удалось открыть файл:" << targetFile;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        qWarning() << "Ошибка парсинга JSON в файле:" << targetFile;
        return false;
    }

    profileObject = doc.object();
    return true;
}

bool ProfileManager::save() {
    QString profileName = profileObject["profileName"].toString();
    if (profileName.isEmpty()) {
        qWarning() << "Имя профиля пустое — не могу сохранить.";
        return false;
    }

    QString baseDir = QCoreApplication::applicationDirPath();
    QString targetDirPath = baseDir + QDir::separator() + "Control Profiles";

    QDir dir;
    if (!dir.exists(targetDirPath)) {
        if (!dir.mkpath(targetDirPath)) {
            qWarning() << "Не удалось создать папку Control Profiles";
            return false;
        }
    }

    QString targetFile = targetDirPath + QDir::separator() + profileName + ".json";
    QFile file(targetFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Не удалось сохранить файл в" << targetFile;
        return false;
    }

    QJsonDocument doc(profileObject);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Файл успешно сохранён в:" << targetFile;
    return true;
}


void ProfileManager::setProfileName(const QString& name) {
    profileObject["profileName"] = name;
}


void ProfileManager::setDevices(const QString& primary, const QString& secondary) {
    QJsonObject devices;
    devices["primary"] = primary;
    devices["secondary"] = secondary;
    profileObject["devices"] = devices;
}

void ProfileManager::addInput(const QString& name, const QString& input, const bool isSecondaryInput) {
    QJsonObject inputObj;
    inputObj["inputName"] = name;
    inputObj["input"] = input;
    inputObj["isSecondary"] = isSecondaryInput;
    inputObj["inversion"] = false;

    QJsonArray inputs = profileObject["inputs"].toArray();
    bool found = false;

    for (int i = 0; i < inputs.size(); ++i) {
        QJsonObject existingObj = inputs[i].toObject();
        if (existingObj["inputName"].toString() == name) {
            inputs[i] = inputObj;
            found = true;
            break;
        }
    }

    if (!found) {
        inputs.append(inputObj);
    }

    profileObject["inputs"] = inputs;
}

void ProfileManager::setInversion(const QString& inputName, bool inversion)
{
    QJsonArray inputs = profileObject["inputs"].toArray();
    bool found = false;

    for (int i = 0; i < inputs.size(); ++i) {
        QJsonObject obj = inputs[i].toObject();
        if (obj["inputName"].toString() == inputName) {
            obj["inversion"] = inversion;
            inputs[i] = obj;
            found = true;
            break;
        }
    }

    if (found) {
        profileObject["inputs"] = inputs;
    }
}


bool ProfileManager::removeInput(const QString& name) {
    QJsonArray inputs = profileObject["inputs"].toArray();
    QJsonArray newInputs;

    bool found = false;
    for (const QJsonValue& value : inputs) {
        QJsonObject obj = value.toObject();
        if (obj["inputName"].toString() != name) {
            newInputs.append(obj);
        } else {
            found = true;
        }
    }

    if (found) {
        profileObject["inputs"] = newInputs;
    }

    return found;
}

QStringList ProfileManager::listAvailableProfiles() {
    QString baseDir = QCoreApplication::applicationDirPath();
    QString targetDirPath = baseDir + QDir::separator() + "Control Profiles";

    QDir dir(targetDirPath);
    QStringList profileFiles = dir.entryList(QStringList() << "*.json", QDir::Files);

    QStringList profileNames;
    for (const QString& file : std::as_const(profileFiles)) {
        profileNames << QFileInfo(file).baseName();
    }

    return profileNames;
}

QJsonObject ProfileManager::getProfile() const {
    return profileObject;
}
