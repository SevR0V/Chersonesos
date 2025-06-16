#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "iplineedit.h"
#include "lineeditutils.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    // QRegularExpression ipPartialRegex(
    //     R"(^(\d{1,3})(\.(\d{1,3}))?(\.(\d{1,3}))?(\.(\d{1,3}))?$)"
    //     );
    // QRegularExpressionValidator *validator = new QRegularExpressionValidator(ipPartialRegex, this);
    // ui->ipLineEdit->setValidator(validator);
    // connect(ui->ipLineEdit, &QLineEdit::textChanged, this, [=]() {
    //     sanitizeIpInput(ui->ipLineEdit);
    // });
    setupIpLineEdit();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::setupIpLineEdit() {
    QLineEdit* old = findChild<QLineEdit*>("ipLineEdit");
    if (!old) return;

    IpLineEdit* ipEdit = new IpLineEdit();
    replaceWidgetInLayout(old, ipEdit);
}

void SettingsDialog::sanitizeIpInput(QLineEdit *lineEdit)
{
    QString text = lineEdit->text();
    QStringList parts = text.split('.');

    bool modified = false;

    for (int i = 0; i < parts.size(); ++i) {
        bool ok;
        int value = parts[i].toInt(&ok);

        if (ok && value > 255) {
            parts[i] = "255";
            modified = true;
        }
    }

    if (modified) {
        int cursorPos = lineEdit->cursorPosition();
        lineEdit->blockSignals(true);
        lineEdit->setText(parts.join('.'));
        lineEdit->setCursorPosition(cursorPos);
        lineEdit->blockSignals(false);
    }
}
