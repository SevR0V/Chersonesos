#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QLineEdit>
#include <QAbstractButton>
#include "profilemanager.h"
#include <QTabWidget>


namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();
    ProfileManager *profileManager;
    void SaveSetting();

signals:
    void settingsChanged();
    void settingsChangedPID();
    void settingsChangedAngle();
private slots:
    void onButtonClicked(QAbstractButton *button);


    void on_pushButton_UpdatePID_clicked();

    void on_pushButton_reset_corn_clicked();

    void on_pushButton_clicked();

private:
    Ui::SettingsDialog *ui;
    void sanitizeIpInput(QLineEdit *lineEdit);
    void setupIpLineEdit();
    void loadSettings();  // Метод для загрузки настроек
    void setupUi(); // Объявление без параметров

QVector<QLineEdit*> m_lineEdits;

};


const QString SETTINGS_FILE = "settings.json";  // Имя файла настроек

#endif // SETTINGSDIALOG_H
