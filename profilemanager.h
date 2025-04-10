#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>

class ProfileManager {
public:
    ProfileManager() = default;

    // Загрузка и сохранение
    bool load(const QString& filePath);
    bool loadFromProfileName(const QString& profileName);
    bool save();

    // Управление данными
    void setProfileName(const QString& name);
    void setDevices(const QString& primary, const QString& secondary);
    void addInput(const QString& name,
                  const QString& primaryInput,
                  const QString& secondaryInput,
                  bool primaryInv,
                  bool secondaryInv);

    // Утилиты
    QStringList listAvailableProfiles();
    QJsonObject getProfile() const;

private:
    QJsonObject profileObject;
};

#endif // PROFILEMANAGER_H
