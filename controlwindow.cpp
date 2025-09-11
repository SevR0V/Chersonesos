#include "controlwindow.h"
#include "ui_controlwindow.h"
#include "customlineedit.h"
#include <QDebug>
#include <QTimer>
#include <QJsonArray>
#include "lineeditutils.h"
#include "gamepadworker.h"


ControlWindow::ControlWindow(GamepadWorker *worker, ProfileManager *profileManager, QWidget *parent)
    : QWidget(parent)
    , isJoyListenerFinished(false),
    ui(new Ui::ControlWindow),
    profileManager(profileManager),
    _worker(worker)
{
    ui->setupUi(this);

    //GUI
    replaceLineEdits(ui->scrollArea->widget());
    isJoyListenerFinished = true;
    progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &ControlWindow::updateProgress);
    QList<QCheckBox*> checkBoxes = findChildren<QCheckBox*>();
    for (QCheckBox* checkBox : checkBoxes) {
        connect(checkBox, &QCheckBox::toggled, this, &ControlWindow::onInversionCBvalueChange);
    }
    ui->profileList->addItems(profileManager->listAvailableProfiles());
    connect(ui->loadProfileBut, &QPushButton::clicked, this, &ControlWindow::onLoadProfileBtnClick);
    //джойстик
    // profileManager = new ProfileManager;
    currentPrimaryDeviceName = "No Device";
    currentSecondaryDeviceName = "No Device";

    connect(ui->primaryDeviceList, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlWindow::onPrimaryDeviceChanged);
    connect(ui->secondaryDeviceList, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlWindow::onSecondaryDeviceChanged);
    connect(ui->saveProfileBut, &QPushButton::clicked,
            this, &ControlWindow::onSaveButtonPressed);

    QTimer *updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &ControlWindow::checkForDeviceChanges);
    updateTimer->start(1000);


    connect(_worker, &GamepadWorker::primaryButtonPressed, this, &ControlWindow::onPrimaryButtonPressed);
    connect(_worker, &GamepadWorker::primaryAxisMoved, this, &ControlWindow::onPrimaryAxisMoved);
    connect(_worker, &GamepadWorker::secondaryButtonPressed, this, &ControlWindow::onSecondaryButtonPressed);
    connect(_worker, &GamepadWorker::secondaryAxisMoved, this, &ControlWindow::onSecondaryAxisMoved);
    connect(_worker, &GamepadWorker::primaryHatPressed, this, &ControlWindow::onPrimaryHatPressed);
    connect(_worker, &GamepadWorker::secondaryHatPressed, this, &ControlWindow::onSecondaryHatPressed);
    connect(_worker, &GamepadWorker::deviceListUpdated, this, &ControlWindow::updateDeviceList);
    checkForDeviceChanges();

    //Тут подключаем джойстик после загрузки из конфига
    _worker->setPrimaryDevice(currentPrimaryDeviceName);
    ui->primaryDeviceList->setCurrentIndex(ui->primaryDeviceList->findText(currentPrimaryDeviceName));
    _worker->setSecondaryDevice(currentSecondaryDeviceName);
    ui->secondaryDeviceList->setCurrentIndex(ui->secondaryDeviceList->findText(currentSecondaryDeviceName));
}

ControlWindow::~ControlWindow()
{
    delete ui;
}

// Функции GUI
void ControlWindow::replaceLineEdits(QWidget *widget) {
    QList<QLineEdit*> edits = widget->findChildren<QLineEdit*>();

    for (QLineEdit* lineEdit : edits) {
        CustomLineEdit* custom = new CustomLineEdit();
        if (replaceWidgetInLayout(lineEdit, custom)) {
            connect(custom, &CustomLineEdit::leftClicked, this, [this, custom]() {
                onLineEditLeftClicked(custom->objectName());
            });
            connect(custom, &CustomLineEdit::rightClicked, this, [this, custom]() {
                onLineEditRightClicked(custom->objectName());
            });
        } else {
            delete custom;
        }
    }
}

void ControlWindow::onLineEditLeftClicked(QString name) {
    if(! isJoyListenerFinished) return;
    // qDebug() << "Левая кнопка нажата на LineEdit с именем:" << name;
    if (name.toLower().contains("primary") && currentPrimaryDeviceName.toLower() == "no device") return;
    if (name.toLower().contains("secondary") && currentSecondaryDeviceName.toLower() == "no device") return;
    // qDebug() << currentPrimaryDeviceName.toLower();
    startProgressCountdown();
    activeInputName = name;
}

void ControlWindow::onLineEditRightClicked(QString name) {
    // qDebug() << "Правая кнопка нажата на LineEdit с именем:" << name;
    if(isJoyListenerFinished){
        CustomLineEdit* activeLineEdit = this->findChild<CustomLineEdit*>(name);
        if(! activeLineEdit) return;
        activeLineEdit->setText("");
        profileManager->removeInput(name);
    } else {
        stopProgressCountdown();
    }
    activeInputName = "";
}

void ControlWindow::startProgressCountdown() {
    if (!progressTimer->isActive()) {
        ui->inputProgressBar->setValue(5000);
        isJoyListenerFinished = false;
        progressTimer->start(10);
    }
}

void ControlWindow::stopProgressCountdown() {
    if (progressTimer->isActive()) {
        progressTimer->stop();
        isJoyListenerFinished = true;
        ui->inputProgressBar->setValue(0);
        activeInputName = "";
    }
}

void ControlWindow::updateProgress() {
    int currentValue = ui->inputProgressBar->value();
    if (currentValue > 0) {
        ui->inputProgressBar->setValue(currentValue - 10);
    } else {
        progressTimer->stop();
        isJoyListenerFinished = true;
    }
}
// Функции джойстика
void ControlWindow::updateDeviceList(const QStringList &deviceNames)
{
    QString oldPrimaryName = ui->primaryDeviceList->currentText();
    QString oldSecondaryName = ui->secondaryDeviceList->currentText();

    ui->primaryDeviceList->clear();
    ui->secondaryDeviceList->clear();

    ui->primaryDeviceList->addItems(deviceNames);
    ui->secondaryDeviceList->addItems(deviceNames);

    int primaryIndex = ui->primaryDeviceList->findText(oldPrimaryName);
    if (primaryIndex >= 0) {
        ui->primaryDeviceList->setCurrentIndex(primaryIndex);
    } else {
        ui->primaryDeviceList->setCurrentIndex(0);
    }

    int secondaryIndex = ui->secondaryDeviceList->findText(oldSecondaryName);
    if (secondaryIndex >= 0) {
        ui->secondaryDeviceList->setCurrentIndex(secondaryIndex);
    } else {
        ui->secondaryDeviceList->setCurrentIndex(0);
    }

    // QString profileName = ui->profileList->currentText();
    // if (profileManager->loadFromProfileName(profileName)){
    //     qDebug() << "profile " << profileName << " loaded";
    // }


    onLoadProfileBtnClick(); //пофиксить логику

    _worker->resetDeviceListChanged();
}

void ControlWindow::onPrimaryDeviceChanged(int index)
{
    QString deviceName = ui->primaryDeviceList->itemText(index);
    qDebug() << "Primary device changed to:" << deviceName;
    _worker->setPrimaryDevice(deviceName);
    currentPrimaryDeviceName = deviceName;
}

void ControlWindow::onSecondaryDeviceChanged(int index)
{
    QString deviceName = ui->secondaryDeviceList->itemText(index);
    qDebug() << "Secondary device changed to:" << deviceName;
    _worker->setSecondaryDevice(deviceName);
    currentSecondaryDeviceName = deviceName;
}

void ControlWindow::checkForDeviceChanges()
{
    if (_worker->hasDeviceListChanged()) {
        qDebug() << "Device list changed, requesting update";
        _worker->updateDeviceList();
    }
}

void ControlWindow::onSaveButtonPressed()
{
    profileManager->setProfileName(ui->profileNameVal->text());
    profileManager->setDevices(currentPrimaryDeviceName, currentSecondaryDeviceName);
    profileManager->save();
    ui->profileList->clear();
    ui->profileList->addItems(profileManager->listAvailableProfiles());
    ui->profileList->setCurrentText(ui->profileNameVal->text());
}

void ControlWindow::onInversionCBvalueChange(bool checked){
    QCheckBox* checkBox = qobject_cast<QCheckBox*>(sender());
    if (checkBox) {
        QString inputName = checkBox->objectName().remove("Inv");
        profileManager->setInversion(inputName, checked);
    }
}

void ControlWindow::profileActionDetected(QString inputType, int inputIdx)
{
    if(activeInputName == "") return;
    if(isJoyListenerFinished) return;
    CustomLineEdit* activeLineEdit = this->findChild<CustomLineEdit*>(ControlWindow::activeInputName);
    if(! activeLineEdit) return;
    // qDebug() << activeLineEdit;
    QString input = inputType + " " + QString::number(inputIdx);
    activeLineEdit->setText(input);
    bool isSecondary = activeInputName.contains("secondary", Qt::CaseInsensitive);
    ControlWindow::profileManager->addInput(activeInputName, input, isSecondary);
    // qDebug() << activeInputName << " " << input << " " << isSecondary << false;
    stopProgressCountdown();
}

void ControlWindow::onLoadProfileBtnClick()
{
    QString profileName = ui->profileList->currentText();
    loadProfile(profileName);
}

void ControlWindow::loadProfile(const QString &profileName){
    if (profileManager->loadFromProfileName(profileName)){
        qDebug() << "profile " << profileName << " loaded";
    }
    if(ui->profileList->currentText()!= profileName){
        ui->profileList->setCurrentText(profileName);
    }
    updateProfileOnGUI();
}

void ControlWindow::updateProfileOnGUI()
{
    QJsonObject controlProfile = profileManager->getProfile();
    QJsonValue profileName = controlProfile["profileName"];
    QJsonObject devices = controlProfile["devices"].toObject();
    QJsonArray inputs = controlProfile["inputs"].toArray();
    ui->profileNameVal->setText(profileName.toString());
    currentPrimaryDeviceName = devices["primary"].toString();
    currentSecondaryDeviceName = devices["secondary"].toString();
    if (ui->primaryDeviceList->findText(currentPrimaryDeviceName) == -1){
        ui->primaryDeviceList->addItem("[offline]" + currentPrimaryDeviceName);
        ui->primaryDeviceList->setCurrentIndex(ui->primaryDeviceList->findText("[offline]" + currentPrimaryDeviceName));
        currentPrimaryDeviceName = "No Device";
        _worker->setPrimaryDevice("No Device");
    } else {
        ui->primaryDeviceList->setCurrentIndex(ui->primaryDeviceList->findText(currentPrimaryDeviceName));
        _worker->setSecondaryDevice(currentPrimaryDeviceName);
    }
    if (ui->secondaryDeviceList->findText(currentSecondaryDeviceName) == -1){
        ui->secondaryDeviceList->addItem("[offline]" + currentSecondaryDeviceName);
        ui->secondaryDeviceList->setCurrentIndex(ui->secondaryDeviceList->findText("[offline]" + currentSecondaryDeviceName));
        currentSecondaryDeviceName = "No Device";
        _worker->setSecondaryDevice("No Device");
    } else {
        _worker->setSecondaryDevice(currentSecondaryDeviceName);
        ui->secondaryDeviceList->setCurrentIndex(ui->secondaryDeviceList->findText(currentSecondaryDeviceName));
    }

    QList<QCheckBox*> checkBoxes = findChildren<QCheckBox*>();
    for (QCheckBox* checkBox : checkBoxes) {
        checkBox->setChecked(false);
    }


    QList<CustomLineEdit*> lineEdits = findChildren<CustomLineEdit*>();
    for (CustomLineEdit* lineEdit : lineEdits) {
        lineEdit->setText("");
    }

    for (const QJsonValue& value : inputs) {
        QJsonObject obj = value.toObject();
        QString name = obj["inputName"].toString();
        QString input = obj["input"].toString();
        bool inverted = obj["inversion"].toBool();
        // qDebug() << name;
        CustomLineEdit* activeLineEdit = this->findChild<CustomLineEdit*>(name);
        QCheckBox* activeCheckBox = this->findChild<QCheckBox*>(name+"Inv");
        if(activeLineEdit){
            activeLineEdit->setText(input);
        }
        if(activeCheckBox){
            activeCheckBox->setChecked(inverted);
        }
    }
}

void ControlWindow::onPrimaryButtonPressed(int button)
{
    // qDebug() << "Primary Button" << button << "pressed";
    if(! activeInputName.contains("primary", Qt::CaseInsensitive)) return;
    if(! activeInputName.contains("but", Qt::CaseInsensitive)) return;
    ControlWindow::profileActionDetected("button", button);
}

void ControlWindow::onPrimaryAxisMoved(int axis, Sint16 value)
{
    // qDebug() << "Primary Axis" << axis << ":" << value;
    if(! activeInputName.contains("primary", Qt::CaseInsensitive)) return;
    if(activeInputName.contains("but", Qt::CaseInsensitive)) return;
    ControlWindow::profileActionDetected("axis", axis);
}

void ControlWindow::onPrimaryHatPressed(int hat, QString direction)
{
    // qDebug() << "Primary Button" << button << "pressed";
    if(! activeInputName.contains("primary", Qt::CaseInsensitive)) return;
    if(! activeInputName.contains("but", Qt::CaseInsensitive)) return;
    ControlWindow::profileActionDetected("hat_" + direction, hat);
}

void ControlWindow::onSecondaryButtonPressed(int button)
{
    // qDebug() << "Secondary Button" << button << "pressed";
    if(! activeInputName.contains("secondary", Qt::CaseInsensitive)) return;
    if(! activeInputName.contains("but", Qt::CaseInsensitive)) return;
    ControlWindow::profileActionDetected("button", button);
}

void ControlWindow::onSecondaryAxisMoved(int axis, Sint16 value)
{
    // qDebug() << "Secondary Axis" << axis << ":" << value;
    if(! activeInputName.contains("secondary", Qt::CaseInsensitive)) return;
    if(activeInputName.contains("but", Qt::CaseInsensitive)) return;
    ControlWindow::profileActionDetected("axis", axis);
}

void ControlWindow::onSecondaryHatPressed(int hat, QString direction)
{
    // qDebug() << "Secondary Button" << button << "pressed";
    if(! activeInputName.contains("secondary", Qt::CaseInsensitive)) return;
    if(! activeInputName.contains("but", Qt::CaseInsensitive)) return;
    ControlWindow::profileActionDetected("hat_" + direction, hat);
}
