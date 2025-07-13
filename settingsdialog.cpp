#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "iplineedit.h"
#include "lineeditutils.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QKeyEvent>
#include "SettingsManager.h"
#include <QRegularExpressionValidator>
#include <QShortcut>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    ui->ipLineEdit->setPlaceholderText("192.168.1.1");
    // Добавляем валидатор к существующему QLineEdit
    QRegularExpression ipRegex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}"
                               "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    QRegularExpressionValidator *ipValidator = new QRegularExpressionValidator(ipRegex, this);
    ui->ipLineEdit->setValidator(ipValidator);

    ui->portLineEdit->setPlaceholderText("1337");
    QIntValidator *validator = new QIntValidator(this);
    ui->portLineEdit->setValidator(validator);

    // загрузка значений настройки в поля формы.
    loadSettings();
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &SettingsDialog::onButtonClicked);

    // контроль клавиши F2
    bool enabled=false;
    ui->tabWidget->setTabEnabled(2, enabled);
    QShortcut *shortcutF2 = new QShortcut(QKeySequence(Qt::Key_F2), this);
    connect(shortcutF2, &QShortcut::activated, this, [this, &enabled]() {
        qDebug() << "F2 pressed via shortcut";
        enabled= !enabled;
        ui->tabWidget->setTabEnabled(2, enabled);
    });
}
void saveLineEditSettings(const QString& key, QLineEdit* lineEdit) {
    SettingsManager::instance().setString(key, lineEdit->text());
}

void saveSpinBoxSettings(const QString& key, QSpinBox* spinBox) {
    SettingsManager::instance().setInt(key, spinBox->value());
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}
void SettingsDialog::loadSettings()
{
   // qDebug() << "start loadSettings:";

    ui->portLineEdit->setText(SettingsManager::instance().getString("portEdit"));
    ui->ipLineEdit->setText(SettingsManager::instance().getString("ip"));
    ui->spinBoxPower->setValue(SettingsManager::instance().getInt("spinBoxPower"));
    ui->lineEditSetting1->setText(SettingsManager::instance().getString("lineEditSetting1"));
    ui->lineEditSetting2->setText(SettingsManager::instance().getString("lineEditSetting2"));
    ui->lineEditSetting3->setText(SettingsManager::instance().getString("lineEditSetting3"));
    ui->lineEditSetting4->setText(SettingsManager::instance().getString("lineEditSetting4"));

    ui->comboBoxCam->setCurrentIndex(SettingsManager::instance().getInt("comboBoxCam"));
    ui->spinBoxTimeCam->setValue(SettingsManager::instance().getInt("spinBoxTimeCam"));
    ui->spinBoxMaxCam->setValue(SettingsManager::instance().getInt("spinBoxMaxCam"));

    ui->lineEdiRollkP->setText(SettingsManager::instance().getString("RollkP"));
    ui->lineEdiRollkI->setText(SettingsManager::instance().getString("RollkI"));
    ui->lineEdiRollkD->setText(SettingsManager::instance().getString("RollkD"));
    ui->lineEdiPitchkP->setText(SettingsManager::instance().getString("PitchkP"));
    ui->lineEdiPitchkI->setText(SettingsManager::instance().getString("PitchkI"));
    ui->lineEdiPitchkD->setText(SettingsManager::instance().getString("PitchkD"));
    ui->lineEdiYawkP->setText(SettingsManager::instance().getString("YawkP"));
    ui->lineEdiYawkI->setText(SettingsManager::instance().getString("YawkI"));
    ui->lineEdiYawkD->setText(SettingsManager::instance().getString("YawkD"));
    ui->lineEdiDepthkP->setText(SettingsManager::instance().getString("DepthkP"));
    ui->lineEdiDepthkI->setText(SettingsManager::instance().getString("DepthkI"));
    ui->lineEdiDepthkD->setText(SettingsManager::instance().getString("DepthkD"));

   // qDebug() << ui->comboBoxCam->currentIndex();
}

void SettingsDialog::onButtonClicked(QAbstractButton *button) {
    // Определяем какая кнопка была нажата
    if (button == ui->buttonBox->button(QDialogButtonBox::Ok)) {
       // qDebug() << "Нажата OK";

        // Изменение настроек
        //saveLineEditSettings("ip", ui->ipLineEdit);
        //saveSpinBoxSettings("spinBoxPower", ui->spinBoxPower);
        SettingsManager::instance().setString("ip", ui->ipLineEdit->text());
        SettingsManager::instance().setString("portEdit", ui->portLineEdit->text());
        SettingsManager::instance().setInt("spinBoxPower", ui->spinBoxPower->value());

        SettingsManager::instance().setString("lineEditSetting1", ui->lineEditSetting1->text());
        SettingsManager::instance().setString("lineEditSetting2", ui->lineEditSetting2->text());
        SettingsManager::instance().setString("lineEditSetting3", ui->lineEditSetting3->text());
        SettingsManager::instance().setString("lineEditSetting4", ui->lineEditSetting4->text());

        SettingsManager::instance().setInt("comboBoxCam", ui->comboBoxCam->currentIndex());
        SettingsManager::instance().setInt("spinBoxTimeCam", ui->spinBoxTimeCam->value());
        SettingsManager::instance().setInt("spinBoxMaxCam", ui->spinBoxMaxCam->value());

        SettingsManager::instance().setString("RollkP", ui->lineEdiRollkP->text());
        SettingsManager::instance().setString("RollkI", ui->lineEdiRollkI->text());
        SettingsManager::instance().setString("RollkD", ui->lineEdiRollkD->text());
        SettingsManager::instance().setString("PitchkP", ui->lineEdiPitchkP->text());
        SettingsManager::instance().setString("PitchkI", ui->lineEdiPitchkI->text());
        SettingsManager::instance().setString("PitchkD", ui->lineEdiPitchkD->text());
        SettingsManager::instance().setString("YawkP", ui->lineEdiYawkP->text());
        SettingsManager::instance().setString("YawkI", ui->lineEdiYawkI->text());
        SettingsManager::instance().setString("YawkD", ui->lineEdiYawkD->text());
        SettingsManager::instance().setString("DepthkP", ui->lineEdiDepthkP->text());
        SettingsManager::instance().setString("DepthkI", ui->lineEdiDepthkI->text());
        SettingsManager::instance().setString("DepthkD", ui->lineEdiDepthkD->text());

        SettingsManager::instance().saveToFile("settings.json");
    }
}







