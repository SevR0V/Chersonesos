#include "overlaywidget.h"

OverlayWidget::OverlayWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents); // Прозрачный для событий мыши
    setStyleSheet("background-color: transparent;"); // Полностью прозрачный фон
}

void OverlayWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Рисуем оверлей на всей доступной области виджета
    //painter.setBrush(QBrush(QColor(255, 0, 0, 100))); // Будет красить
    painter.drawRect(rect()); // Используем rect() для получения текущих размеров виджета

    // Пример оверлея: красная линия от угла к углу
    painter.setPen(QPen(Qt::red, 2));
    painter.drawLine(10, 10, width() - 10, height() - 10);

    // Пример оверлея: текст в центре
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12));
    painter.drawText(rect(), Qt::AlignCenter, "Overlay Example");
}
