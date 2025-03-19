#include "controlwindow.h"
#include "ui_controlwindow.h"
#include "customlineedit.h"
#include <QDebug>
#include <QTimer>

ControlWindow::ControlWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ControlWindow)
{
    ui->setupUi(this);

    //GUI
    replaceLineEdits(ui->scrollArea->widget());
    isJoyListenerFinished = false;
    progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &ControlWindow::updateProgress);
    //джойстик
    currentPrimaryName = "No Device";
    currentSecondaryName = "No Device";

    connect(ui->primaryDeviceList, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlWindow::onPrimaryDeviceChanged);
    connect(ui->secondaryDeviceList, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlWindow::onSecondaryDeviceChanged);

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
    connect(worker, &GamepadWorker::deviceListUpdated, this, &ControlWindow::updateDeviceList);
    connect(this, &ControlWindow::destroyed, worker, &GamepadWorker::stop);
    connect(workerThread, &QThread::finished, worker, &GamepadWorker::deleteLater);

    workerThread->start();
    checkForDeviceChanges();

    //Тут подключаем джойстик после загрузки из конфига
    worker->setSecondaryDevice(currentSecondaryName);
    ui->secondaryDeviceList->setCurrentIndex(ui->secondaryDeviceList->findText(currentSecondaryName));
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

        // Если элемент — это layout, проверяем его рекурсивно
        if (QLayout* subLayout = item->layout()) {
            QLayout* found = findParentLayout(widget, subLayout);
            if (found) {
                return found;
            }
        }

        // Если элемент — это виджет, проверяем его layout
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

            // Создаем новый CustomLineEdit
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

            // Подключаем сигналы
            connect(customLineEdit, &CustomLineEdit::leftClicked, this, [this, customLineEdit]() {
                onLineEditLeftClicked(customLineEdit->objectName());
            });
            connect(customLineEdit, &CustomLineEdit::rightClicked, this, [this, customLineEdit]() {
                onLineEditRightClicked(customLineEdit->objectName());
            });

            // Приводим QLayout к QBoxLayout, так как у вас только QVBoxLayout и QHBoxLayout
            if (QBoxLayout* boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                layout->removeWidget(lineEdit);  // Удаляем старый виджет
                boxLayout->insertWidget(index, customLineEdit);  // Вставляем новый на ту же позицию
            } else {
                delete customLineEdit;  // Очищаем память, если не удалось вставить
                continue;
            }

            delete lineEdit;  // Удаляем старый QLineEdit
        }
    }
}

void ControlWindow::onLineEditLeftClicked(QString name) {
    qDebug() << "Левая кнопка нажата на LineEdit с именем:" << name;
    startProgressCountdown();
}

void ControlWindow::onLineEditRightClicked(QString name) {
    qDebug() << "Правая кнопка нажата на LineEdit с именем:" << name;
    stopProgressCountdown();
}

void ControlWindow::startProgressCountdown() {
    if (!progressTimer->isActive()) { // Проверяем, не запущен ли уже таймер
        ui->inputProgressBar->setValue(5000); // Сбрасываем прогресс-бар на максимум
        isJoyListenerFinished = false;              // Сбрасываем флаг
        progressTimer->start(10);                // Запускаем таймер (10 мс интервал)
    }
}

void ControlWindow::stopProgressCountdown() {
    if (progressTimer->isActive()) { // Проверяем, активен ли таймер
        progressTimer->stop();       // Останавливаем таймер
        isJoyListenerFinished = true;
        ui->inputProgressBar->setValue(0);
    }
}

void ControlWindow::updateProgress() {
    int currentValue = ui->inputProgressBar->value();
    if (currentValue > 0) {
        ui->inputProgressBar->setValue(currentValue - 10); // Уменьшение на 10 за 10 мс
    } else {
        progressTimer->stop(); // Останавливаем таймер
        isJoyListenerFinished = true; // Переключаем флаг
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
    currentPrimaryName = deviceName;
}

void ControlWindow::onSecondaryDeviceChanged(int index)
{
    QString deviceName = ui->secondaryDeviceList->itemText(index);
    qDebug() << "Secondary device changed to:" << deviceName;
    worker->setSecondaryDevice(deviceName);
    currentSecondaryName = deviceName;
}

void ControlWindow::checkForDeviceChanges()
{
    if (worker->hasDeviceListChanged()) {
        qDebug() << "Device list changed, requesting update";
        worker->updateDeviceList();
    }
}

void ControlWindow::onPrimaryButtonPressed(int button)
{
    qDebug() << "Primary Button" << button << "pressed";
}

void ControlWindow::onPrimaryAxisMoved(int axis, Sint16 value)
{
    qDebug() << "Primary Axis" << axis << ":" << value;
}

void ControlWindow::onSecondaryButtonPressed(int button)
{
    qDebug() << "Secondary Button" << button << "pressed";
}

void ControlWindow::onSecondaryAxisMoved(int axis, Sint16 value)
{
    qDebug() << "Secondary Axis" << axis << ":" << value;
}
