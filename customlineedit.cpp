#include "customlineedit.h"
#include <QDebug>

CustomLineEdit::CustomLineEdit(QWidget *parent) : QLineEdit(parent) {
}

void CustomLineEdit::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit leftClicked(objectName());
    } else if (event->button() == Qt::RightButton) {
        emit rightClicked(objectName());
    }
    QLineEdit::mousePressEvent(event);
}
