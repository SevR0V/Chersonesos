#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QResizeEvent>

void setMasterButtonState(QPushButton *button, const bool masterState, const bool isPanelHidden);
void setRecordButtonState(QPushButton *button, const bool isRecording, const bool isPanelHidden);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    controlsWindow = new ControlWindow;
    settingsDialog = new SettingsDialog;

    // *controlsWindow->profileManager = *MainWindow::profileManager;
    connect(ui->controlsButton, &QPushButton::pressed, [this]() {controlsWindow->show();});
    connect(ui->settingsButton, &QPushButton::pressed, [this]() {settingsDialog->show();});
    connect(ui->startRecordButton, &QPushButton::pressed, this, &MainWindow::startRecord);
    connect(ui->hideShowButton, &QPushButton::pressed, this, &MainWindow::showHideLeftPanel);
    connect(ui->masterButton, &QPushButton::pressed, this, &MainWindow::masterSwitch);
    ui->videoWidget->setStyleSheet("background-color: lightblue");

    ui->powerGroupBox->setStyleSheet("QGroupBox {""border: 0px; ""}");

    QIcon locked(":/Resources/Icons/lock_closed.ico");
    // unlocked.addFile(":/Resources/Icons/lock_open.ico");
    ui->masterButton->setIcon(locked);
    ui->masterButton->setIconSize(QSize(28, 28));

    QIcon leftArrow(":/Resources/Icons/left_arrow.ico");
    ui->hideShowButton->setIcon(leftArrow);
    ui->hideShowButton->setIconSize(QSize(28, 28));

    QIcon icon(":/Resources/Icons/circle_black.ico");
    ui->startRecordButton->setIcon(icon);
    ui->hideShowButton->setIconSize(QSize(14, 14));

    isPanelHidden = false;

    isRecording = false;
    masterState = false;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showHideLeftPanel()
{
    isPanelHidden = !isPanelHidden;
    if(isPanelHidden){
        QIcon rightArrow(":/Resources/Icons/right_arrow.ico");
        ui->hideShowButton->setIcon(rightArrow);

        ui->masterButton->setText("");
        ui->takeStereoframeButton->setText("");
        ui->startRecordButton->setText("");
        ui->controlsButton->setText("");
        ui->settingsButton->setText("");

        ui->onlineLable->hide();
        ui->recordStereoCheckBox->hide();
        ui->powerGroupBox->hide();
        ui->rovStateLabel->hide();
        ui->powerLabel->hide();
        ui->recordLabel->hide();
        ui->settingsLabel->hide();
        ui->powerBotLine->hide();
    } else {
        QIcon leftArrow(":/Resources/Icons/left_arrow.ico");
        ui->hideShowButton->setIcon(leftArrow);

        setMasterButtonState(ui->masterButton, masterState, isPanelHidden);
        ui->takeStereoframeButton->setText("Сделать стереокадр");
        setRecordButtonState(ui->startRecordButton,isRecording, isPanelHidden);
        ui->controlsButton->setText("Управление");
        ui->settingsButton->setText("Настройки");
        ui->onlineLable->setText("Не в сети");

        ui->onlineLable->show();
        ui->recordStereoCheckBox->show();
        ui->powerGroupBox->show();
        ui->powerBotLine->show();
        ui->rovStateLabel->show();
        ui->powerLabel->show();
        ui->recordLabel->show();
        ui->settingsLabel->show();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{

}

void MainWindow::masterSwitch()
{
    masterState = !masterState;
    setMasterButtonState(ui->masterButton, masterState, isPanelHidden);
}

void setMasterButtonState(QPushButton *button, const bool masterState, const bool isPanelHidden)
{
    if(masterState)
    {
        if(! isPanelHidden)
            button->setText("Заблокировать ТНПА");
        QIcon unlocked(":/Resources/Icons/lock_open.ico");
        button->setIcon(unlocked);

    } else {
        if(! isPanelHidden)
            button->setText("Разблокировать ТНПА");
        QIcon locked(":/Resources/Icons/lock_closed.ico");
        button->setIcon(locked);
    }
}

void MainWindow::startRecord()
{
    ui->recordStereoCheckBox->setDisabled(!isRecording);
    isRecording = !isRecording;

    setRecordButtonState(ui->startRecordButton,isRecording, isPanelHidden);

    bool isStereoRecording = ui->recordStereoCheckBox->isChecked();
}

void setRecordButtonState(QPushButton *button, const bool isRecording, const bool isPanelHidden)
{
    if(isRecording)
    {
        if(! isPanelHidden)
            button->setText("Остановить запись");
        QIcon icon(":/Resources/Icons/circle_red.ico");
        button->setIcon(icon);
    } else {
        if(! isPanelHidden)
            button->setText("Записать видео");
        QIcon icon(":/Resources/Icons/circle_black.ico");
        button->setIcon(icon);
    }
}
