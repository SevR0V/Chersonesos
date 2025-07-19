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
    ui->groupBox_5->setVisible(enabled);
    QShortcut *shortcutF2 = new QShortcut(QKeySequence(Qt::Key_F2), this);
    connect(shortcutF2, &QShortcut::activated, this, [this, &enabled]() {
        qDebug() << "F2 pressed via shortcut";
        enabled= !enabled;

        ui->groupBox_5->setVisible(enabled);
    });
    // Проверка сигнала
    connect(this, &SettingsDialog::settingsChanged, []() {
        qDebug() << "Сигнал settingsChanged() был вызван!";
    });
    // Проверка сигнала
    connect(this, &SettingsDialog::settingsChangedPID, []() {
        qDebug() << "Сигнал settingsChangedPID() был вызван!";
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

    ui->comboBoxCam->setCurrentIndex(SettingsManager::instance().getInt("comboBoxCam"));
    ui->spinBoxTimeCam->setValue(SettingsManager::instance().getInt("spinBoxTimeCam"));
    ui->spinBoxMaxCam->setValue(SettingsManager::instance().getInt("spinBoxMaxCam"));
    ui->spinBox_Cam_angle_minus->setValue(SettingsManager::instance().getInt("Cam_angle_minus"));
    ui->spinBox_Cam_angle_plus->setValue(SettingsManager::instance().getInt("Cam_angle_plus"));


    ui->doubleSpinBox_kP_Roll->setValue(SettingsManager::instance().getDouble("RollkP"));
    ui->doubleSpinBox_kI_Roll->setValue(SettingsManager::instance().getDouble("RollkI"));
    ui->doubleSpinBox_kD_Roll->setValue(SettingsManager::instance().getDouble("RollkD"));
    ui->doubleSpinBox_kP_Pitch->setValue(SettingsManager::instance().getDouble("PitchkP"));
    ui->doubleSpinBox_kI_Pitch->setValue(SettingsManager::instance().getDouble("PitchkI"));
    ui->doubleSpinBox_kD_Pitch->setValue(SettingsManager::instance().getDouble("PitchkD"));
    ui->doubleSpinBox_kP_Yaw->setValue(SettingsManager::instance().getDouble("YawkP"));
    ui->doubleSpinBox_kI_Yaw->setValue(SettingsManager::instance().getDouble("YawkI"));
    ui->doubleSpinBox_kD_Yaw->setValue(SettingsManager::instance().getDouble("YawkD"));
    ui->doubleSpinBox_kP_Depth->setValue(SettingsManager::instance().getDouble("DepthkP"));
    ui->doubleSpinBox_kI_Depth->setValue(SettingsManager::instance().getDouble("DepthkI"));
    ui->doubleSpinBox_kD_Depth->setValue(SettingsManager::instance().getDouble("DepthkD"));
    emit settingsChanged();
   // qDebug() << ui->comboBoxCam->currentIndex();
}
void SettingsDialog::SaveSetting()
{
    // Изменение настроек
    //saveLineEditSettings("ip", ui->ipLineEdit);
    //saveSpinBoxSettings("spinBoxPower", ui->spinBoxPower);
    SettingsManager::instance().setString("ip", ui->ipLineEdit->text());
    SettingsManager::instance().setString("portEdit", ui->portLineEdit->text());
    SettingsManager::instance().setInt("spinBoxPower", ui->spinBoxPower->value());


    SettingsManager::instance().setInt("comboBoxCam", ui->comboBoxCam->currentIndex());
    SettingsManager::instance().setInt("spinBoxTimeCam", ui->spinBoxTimeCam->value());
    SettingsManager::instance().setInt("spinBoxMaxCam", ui->spinBoxMaxCam->value());
    SettingsManager::instance().setInt("Cam_angle_minus", ui->spinBox_Cam_angle_minus->value());
    SettingsManager::instance().setInt("Cam_angle_plus", ui->spinBox_Cam_angle_plus->value());

    SettingsManager::instance().setDouble("RollkP", ui->doubleSpinBox_kP_Roll->value());
    SettingsManager::instance().setDouble("RollkI", ui->doubleSpinBox_kI_Roll->value());
    SettingsManager::instance().setDouble("RollkD", ui->doubleSpinBox_kD_Roll->value());
    SettingsManager::instance().setDouble("PitchkP", ui->doubleSpinBox_kP_Pitch->value());
    SettingsManager::instance().setDouble("PitchkI", ui->doubleSpinBox_kI_Pitch->value());
    SettingsManager::instance().setDouble("PitchkD", ui->doubleSpinBox_kD_Pitch->value());
    SettingsManager::instance().setDouble("YawkP", ui->doubleSpinBox_kP_Yaw->value());
    SettingsManager::instance().setDouble("YawkI", ui->doubleSpinBox_kI_Yaw->value());
    SettingsManager::instance().setDouble("YawkD", ui->doubleSpinBox_kD_Yaw->value());
    SettingsManager::instance().setDouble("DepthkP", ui->doubleSpinBox_kP_Depth->value());
    SettingsManager::instance().setDouble("DepthkI", ui->doubleSpinBox_kI_Depth->value());
    SettingsManager::instance().setDouble("DepthkD", ui->doubleSpinBox_kD_Depth->value());

    SettingsManager::instance().saveToFile("settings.json");
    emit settingsChanged();

}
void SettingsDialog::onButtonClicked(QAbstractButton *button) {
    // Определяем какая кнопка была нажата
    if (button == ui->buttonBox->button(QDialogButtonBox::Ok)) {
       // qDebug() << "Нажата OK";
        SaveSetting();
        // emit settingsChanged();
    }
}

void SettingsDialog::on_pushButton_UpdatePID_clicked()
{
    // qDebug() << "Нажата UpdatePID";
    SaveSetting();
    emit settingsChangedPID();
}

void SettingsDialog::on_pushButton_reset_corn_clicked()
{
    // ui->doubleSpinBox_kP_Roll->setValue(0);
    // ui->doubleSpinBox_kI_Roll->setValue(0);
    // ui->doubleSpinBox_kD_Roll->setValue(0);
    // ui->doubleSpinBox_kP_Pitch->setValue(0);
    // ui->doubleSpinBox_kI_Pitch->setValue(0);
    // ui->doubleSpinBox_kD_Pitch->setValue(0);
    // ui->doubleSpinBox_kP_Yaw->setValue(0);
    // ui->doubleSpinBox_kI_Yaw->setValue(0);
    // ui->doubleSpinBox_kD_Yaw->setValue(0);
    // SaveSetting();
    emit settingsChangedAngle();
}


void SettingsDialog::on_pushButton_clicked()
{

}

