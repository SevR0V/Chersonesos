#include "IpLineEdit.h"
#include <QKeyEvent>
#include <QStringList>

IpLineEdit::IpLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setText("0.0.0.0");
    setMaxLength(15); // max length of xxx.xxx.xxx.xxx
    setCursorPosition(0);
    connect(this, &QLineEdit::textChanged, this, &IpLineEdit::sanitizeInput);
}

void IpLineEdit::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    int pos = cursorPosition();

    if (!event->text().isEmpty()) {
        const QChar ch = event->text().at(0);
        if (!ch.isDigit() && ch != '.' &&
            key != Qt::Key_Backspace &&
            key != Qt::Key_Delete &&
            key != Qt::Key_Tab) {
            return;
        }
    }

    if (key == Qt::Key_Backspace && (event->modifiers() & Qt::ControlModifier)) {
        if (pos > 0 && text().at(pos - 1) == '.') {
            int segIndex = currentSegmentIndex() - 1;
            if (segIndex >= 0) {
                QStringList parts = text().split('.');
                parts[segIndex] = "0";

                blockSignals(true);
                setText(parts.join('.'));

                // Вычисляем позицию начала очищенного сегмента
                int newPos = 0;
                for (int i = 0; i < segIndex; ++i) {
                    newPos += parts[i].length() + 1;
                }
                setCursorPosition(newPos);
                blockSignals(false);
            }
            return;
        }
    }
    // Обработка удаления (точки и символов возле них)
    if (key == Qt::Key_Backspace && pos > 0 && text().at(pos - 1) == '.') {
        // Если слева точка — удаляем символ перед точкой (если есть)
        int jumpBackPos = pos - 2;
        if (jumpBackPos >= 0) {
            QString newText = text();
            newText.remove(jumpBackPos, 1); // удаляем символ перед точкой
            blockSignals(true);
            setText(newText);
            setCursorPosition(jumpBackPos);
            blockSignals(false);
        }
        return;
    }
    if (key == Qt::Key_Delete && pos < text().length() && text().at(pos) == '.') {
        // Просто пропускаем точку вперёд
        setCursorPosition(pos + 1);
        return;
    }

    // Tab / Shift+Tab — переход между сегментами
    if (key == Qt::Key_Tab) {
        if (event->modifiers() & Qt::ShiftModifier)
            moveToPreviousSegment();
        else
            moveToNextSegment();
        return;
    }

    // Обработка ввода точки
    if (key == Qt::Key_Period || event->text() == ".") {
        // Переходить только если справа от курсора уже есть точка
        if (pos < text().length() && text().at(pos) == '.') {
            setCursorPosition(pos + 1);
        }
        // В других случаях — игнорировать
        return;
    }

    // Ввод цифры
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        QString digit = event->text();
        int segIndex = currentSegmentIndex();
        QStringList parts = text().split('.');

        if (segIndex >= 0 && segIndex < parts.size()) {
            int segStart = 0;
            for (int i = 0; i < segIndex; ++i)
                segStart += parts[i].length() + 1;

            int cursorInSegment = cursorPosition() - segStart;

            // Если сегмент равен "0" и курсор в его начале, заменяем 0 на введённую цифру
            if (parts[segIndex] == "0" && cursorInSegment == 0) {
                parts[segIndex] = digit;
                QString newText = parts.join('.');

                blockSignals(true);
                setText(newText);
                setCursorPosition(segStart + 1);
                blockSignals(false);

                return;
            }
        }



        // Обычное поведение и автопереход
        QLineEdit::keyPressEvent(event);

        segIndex = currentSegmentIndex();
        parts = text().split('.');

        if (segIndex < parts.size()) {
            QString segText = parts[segIndex];
            if (segText.length() >= 3 || segText.toInt() > 25) {
                moveToNextSegment();
            }
        }
        return;
    }

    // Остальные клавиши — стандартное поведение
    QLineEdit::keyPressEvent(event);
}

void IpLineEdit::sanitizeInput()
{
    QStringList parts = text().split('.');
    bool modified = false;

    for (int i = 0; i < parts.size(); ++i) {
        bool ok;
        int val = parts[i].toInt(&ok);
        if (!ok || parts[i].isEmpty()) {
            parts[i] = "0";
            modified = true;
        } else if (val > 255) {
            parts[i] = "255";
            modified = true;
        } else {
            parts[i] = QString::number(val); // убираем ведущие нули
        }
    }

    while (parts.size() < 4) {
        parts.append("0");
        modified = true;
    }

    QString clean = parts.join('.');
    if (text() != clean) {
        int pos = cursorPosition();
        blockSignals(true);
        setText(clean);
        setCursorPosition(qMin(pos, clean.length()));
        blockSignals(false);
    }
}

int IpLineEdit::currentSegmentIndex() const
{
    int pos = cursorPosition();
    return text().left(pos).count('.');
}

void IpLineEdit::moveToSegment(int index)
{
    // int offset = 0;
    int dotCount = 0;
    for (int i = 0; i < text().length(); ++i) {
        if (dotCount == index) {
            setCursorPosition(i);
            return;
        }
        if (text()[i] == '.')
            dotCount++;
    }
    setCursorPosition(text().length());
}

void IpLineEdit::moveToNextSegment()
{
    int seg = currentSegmentIndex();
    if (seg < 3)
        moveToSegment(seg + 1);
}

void IpLineEdit::moveToPreviousSegment()
{
    int seg = currentSegmentIndex();
    if (seg > 0)
        moveToSegment(seg - 1);
}
