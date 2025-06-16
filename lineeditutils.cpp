#include "lineeditutils.h"

QLayout* findParentLayout(QWidget* widget, QLayout* layout) {
    if (!layout) return nullptr;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) continue;

        if (item->widget() == widget) return layout;

        if (QLayout* subLayout = item->layout()) {
            if (QLayout* found = findParentLayout(widget, subLayout)) {
                return found;
            }
        }

        if (QWidget* subWidget = item->widget()) {
            if (QLayout* subWidgetLayout = subWidget->layout()) {
                if (QLayout* found = findParentLayout(widget, subWidgetLayout)) {
                    return found;
                }
            }
        }
    }
    return nullptr;
}

QLayout* findContainingLayout(QWidget* widget) {
    if (!widget || !widget->parentWidget()) return nullptr;
    QLayout* layout = widget->parentWidget()->layout();
    return layout ? findParentLayout(widget, layout) : nullptr;
}

bool replaceWidgetInLayout(QWidget* oldWidget, QWidget* newWidget) {
    if (!oldWidget || !newWidget) return false;

    QLayout* layout = findContainingLayout(oldWidget);
    if (!layout) return false;

    int index = -1;
    for (int i = 0; i < layout->count(); ++i) {
        if (layout->itemAt(i)->widget() == oldWidget) {
            index = i;
            break;
        }
    }

    if (index == -1) return false;

    // Переносим основные параметры
    if (auto* oldLineEdit = qobject_cast<QLineEdit*>(oldWidget)) {
        if (auto* newLineEdit = qobject_cast<QLineEdit*>(newWidget)) {
            newLineEdit->setText(oldLineEdit->text());
            newLineEdit->setFont(oldLineEdit->font());
            newLineEdit->setStyleSheet(oldLineEdit->styleSheet());
            newLineEdit->setObjectName(oldLineEdit->objectName());
            newLineEdit->setPlaceholderText(oldLineEdit->placeholderText());
            newLineEdit->setClearButtonEnabled(oldLineEdit->isClearButtonEnabled());
            newLineEdit->setReadOnly(oldLineEdit->isReadOnly());
            newLineEdit->setContextMenuPolicy(oldLineEdit->contextMenuPolicy());
        }
    }

    layout->removeWidget(oldWidget);
    if (auto* box = qobject_cast<QBoxLayout*>(layout)) {
        box->insertWidget(index, newWidget);
    } else {
        layout->addWidget(newWidget);
    }

    oldWidget->deleteLater();
    return true;
}
