#include "controlwindow.h"
#include "ui_controlwindow.h"
#include "customlineedit.h"
#include <QDebug>
#include <QTimer>
#include <QJsonArray>


ControlWindow::ControlWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ControlWindow)
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
    profileManager = new ProfileManager;
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

    workerThread = new QThread(this);
    worker = new GamepadWorker();
    worker->moveToThread(workerThread);

    connect(workerThread, &QThread::started, worker, &GamepadWorker::pollDevices);
    connect(worker, &GamepadWorker::primaryButtonPressed, this, &ControlWindow::onPrimaryButtonPressed);
    connect(worker, &GamepadWorker::primaryAxisMoved, this, &ControlWindow::onPrimaryAxisMoved);
    connect(worker, &GamepadWorker::secondaryButtonPressed, this, &ControlWindow::onSecondaryButtonPressed);
    connect(worker, &GamepadWorker::secondaryAxisMoved, this, &ControlWindow::onSecondaryAxisMoved);
    connect(worker, &GamepadWorker::primaryHatPressed, this, &ControlWindow::onPrimaryHatPressed);
    connect(worker, &GamepadWorker::secondaryHatPressed, this, &ControlWindow::onSecondaryHatPressed);
    connect(worker, &GamepadWorker::deviceListUpdated, this, &ControlWindow::updateDeviceList);
    connect(this, &ControlWindow::destroyed, worker, &GamepadWorker::stop);
    connect(workerThread, &QThread::finished, worker, &GamepadWorker::deleteLater);

    workerThread->start();
    checkForDeviceChanges();

    //Тут подключаем джойстик после загрузки из конфига
    worker->setPrimaryDevice(currentPrimaryDeviceName);
    ui->primaryDeviceList->setCurrentIndex(ui->primaryDeviceList->findText(currentPrimaryDeviceName));
    worker->setSecondaryDevice(currentSecondaryDeviceName);
    ui->secondaryDeviceList->setCurrentIndex(ui->secondaryDeviceList->findText(currentSecondaryDeviceName));
}

ControlWindow::~ControlWindow()
{
    workerThread->quit();
    workerThread->wait();
    delete workerThread;
    delete ui;
}

// Функции GUI
QLayout* findParentLayout(QWidget* widget, QLayout* layout) {
    if (!layout) {
        return nullptr;
    }

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) {
            continue;
        }

        if (item->widget() == widget) {
            return layout;
        }

        if (QLayout* subLayout = item->layout()) {
            QLayout* found = findParentLayout(widget, subLayout);
            if (found) {
                return found;
            }
        }

        if (QWidget* subWidget = item->widget()) {
            if (QLayout* subWidgetLayout = subWidget->layout()) {
                QLayout* found = findParentLayout(widget, subWidgetLayout);
                if (found) {
                    return found;
                }
            }
        }
    }

    return nullptr;
}

QLayout* findContainingLayout(QWidget* widget) {
    QWidget* parent = widget->parentWidget();
    if (!parent) {
        return nullptr;
    }

    QLayout* layout = parent->layout();
    if (!layout) {
        return nullptr;
    }

    return findParentLayout(widget, layout);
}

void ControlWindow::replaceLineEdits(QWidget *widget) {
    QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();

    for (QLineEdit *lineEdit : std::as_const(lineEdits)) {
        QLayout *layout = findContainingLayout(lineEdit);

        if (layout) {
            int index = -1;
            for (int i = 0; i < layout->count(); ++i) {
                QLayoutItem* item = layout->itemAt(i);
                if (item && item->widget() == lineEdit) {
                    index = i;
                    break;
                }
            }

            if (index == -1) {
                continue;
            }

            CustomLineEdit *customLineEdit = new CustomLineEdit();
            customLineEdit->setText(lineEdit->text());
            customLineEdit->setPlaceholderText(lineEdit->placeholderText());
            customLineEdit->setAlignment(lineEdit->alignment());
            customLineEdit->setReadOnly(lineEdit->isReadOnly());
            customLineEdit->setGeometry(lineEdit->geometry());
            customLineEdit->setObjectName(lineEdit->objectName());
            customLineEdit->setStyleSheet(lineEdit->styleSheet());
            customLineEdit->setFont(lineEdit->font());
            customLineEdit->setEchoMode(lineEdit->echoMode());
            customLineEdit->setInputMask(lineEdit->inputMask());
            customLineEdit->setValidator(lineEdit->validator());
            customLineEdit->setMaxLength(lineEdit->maxLength());
            customLineEdit->setClearButtonEnabled(lineEdit->isClearButtonEnabled());
            customLineEdit->setCursorPosition(lineEdit->cursorPosition());
            customLineEdit->setContextMenuPolicy(lineEdit->contextMenuPolicy());

            connect(customLineEdit, &CustomLineEdit::leftClicked, this, [this, customLineEdit]() {
                onLineEditLeftClicked(customLineEdit->objectName());
            });
            connect(customLineEdit, &CustomLineEdit::rightClicked, this, [this, customLineEdit]() {
                onLineEditRightClicked(customLineEdit->objectName());
            });

            if (QBoxLayout* boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                layout->removeWidget(lineEdit);
                boxLayout->insertWidget(index, customLineEdit);
            } else {
                delete customLineEdit;
                continue;
            }

            delete lineEdit;
        }
    }
}

void ControlWindow::onLineEditLeftClicked(QString name) {
    if(! isJoyListenerFinished) return;
    qDebug() << "Левая кнопка нажата на LineEdit с именем:" << name;
    if (name.toLower().contains("primary") && currentPrimaryDeviceName.toLower() == "no device") return;
    if (name.toLower().contains("secondary") && currentSecondaryDeviceName.toLower() == "no device") return;
    qDebug() << currentPrimaryDeviceName.toLower();
    startProgressCountdown();
    activeInputName = name;
}

void ControlWindow::onLineEditRightClicked(QString name) {
    qDebug() << "Правая кнопка нажата на LineEdit с именем:" << name;
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

    worker->resetDeviceListChanged();
}

void ControlWindow::onPrimaryDeviceChanged(int index)
{
    QString deviceName = ui->primaryDeviceList->itemText(index);
    qDebug() << "Primary device changed to:" << deviceName;
    worker->setPrimaryDevice(deviceName);
    currentPrimaryDeviceName = deviceName;
}

void ControlWindow::onSecondaryDeviceChanged(int index)
{
    QString deviceName = ui->secondaryDeviceList->itemText(index);
    qDebug() << "Secondary device changed to:" << deviceName;
    worker->setSecondaryDevice(deviceName);
    currentSecondaryDeviceName = deviceName;
}

void ControlWindow::checkForDeviceChanges()
{
    if (worker->hasDeviceListChanged()) {
        qDebug() << "Device list changed, requesting update";
        worker->updateDeviceList();
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
    qDebug() << activeLineEdit;
    QString input = inputType + " " + QString::number(inputIdx);
    activeLineEdit->setText(input);
    bool isSecondary = activeInputName.contains("secondary", Qt::CaseInsensitive);
    ControlWindow::profileManager->addInput(activeInputName, input, isSecondary);
    qDebug() << activeInputName << " " << input << " " << isSecondary << false;
    stopProgressCountdown();
}

void ControlWindow::onLoadProfileBtnClick()
{
    QString profileName = ui->profileList->currentText();
    if (profileManager->loadFromProfileName(profileName)){
        qDebug() << "profile " << profileName << " loaded";
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
        worker->setPrimaryDevice("No Device");
    } else {
        ui->primaryDeviceList->setCurrentIndex(ui->primaryDeviceList->findText(currentPrimaryDeviceName));
        worker->setSecondaryDevice(currentPrimaryDeviceName);
    }
    if (ui->secondaryDeviceList->findText(currentSecondaryDeviceName) == -1){
        ui->secondaryDeviceList->addItem("[offline]" + currentSecondaryDeviceName);
        ui->secondaryDeviceList->setCurrentIndex(ui->secondaryDeviceList->findText("[offline]" + currentSecondaryDeviceName));
        currentSecondaryDeviceName = "No Device";
        worker->setSecondaryDevice("No Device");
    } else {
        worker->setSecondaryDevice(currentSecondaryDeviceName);
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
        qDebug() << name;
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
    qDebug() << "Primary Axis" << axis << ":" << value;
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
