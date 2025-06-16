#ifndef LINEEDITUTILS_H
#define LINEEDITUTILS_H

#include <QWidget>
#include <QLayout>
#include <QLineEdit>

QLayout* findContainingLayout(QWidget* widget);
bool replaceWidgetInLayout(QWidget* oldWidget, QWidget* newWidget);

#endif // LINEEDIT_UTILS_H
