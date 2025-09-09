// overlaywidget.cpp
#include "overlaywidget.h"
#include "qthread.h"
#include <QScreen>
#include <QWindow>
#include <cmath>  // For std::hypot, std::round, etc.

OverlayWidget::OverlayWidget(QWidget *parent, OverlayFrameInfo* overlayInfo)
    : QWidget(parent), m_overlayInfo(overlayInfo)
{
    setAttribute(Qt::WA_TransparentForMouseEvents); // Прозрачный для событий мыши
    setStyleSheet("background-color: transparent;"); // Полностью прозрачный фон

    ostabEnabled = 0;
    ostabRoll = 0;
    ostabPitch = 0;
    ostabYaw = 0;
    ostabDepth = 0;
    omasterFlag = 0;
    opowerLimit = 0;
    ocamAngle = 0;
    oPitch = 0;
    oRoll = 0;
    oYaw = 0;
    oDepth = 0;
    oPitchSetpoint = 0;
    oRollSetpoint = 0;
    oYawSetpoint = 0;
    oDepthSetpoint = 0;
    oBatLevel = 0;
    prevYaw = 0;
    revolutionCount = 0;
    parentWidget = parent;
    qreal refreshRate = 60;
    QScreen *screen = QGuiApplication::primaryScreen(); // или QApplication::screenAt(...)
    if (screen) {
        refreshRate = screen->refreshRate(); // с Qt 5.14+
        qDebug() << "Refresh rate:" << refreshRate << "Hz";
    }
    frameTimer = new QTimer(this);
    connect(frameTimer, &QTimer::timeout, this, &OverlayWidget::updateOverlay);
    frameTimer->start(1000/refreshRate);

    //pushTimer = new QTimer(this);
    //connect(pushTimer, &QTimer::timeout, this, &OverlayWidget::pushOverlayToQueueSlot);
    //pushTimer->start(50);

    cacheValid = false;
}


void drawCrosshair(QPainter* painter, const QPoint& center, int size, int lineWidth, int gap, const QColor& color) {
    if (!painter || size <= 0 || lineWidth <= 0)
        return;

    QPen pen(color);
    pen.setWidth(lineWidth);
    pen.setCapStyle(Qt::FlatCap);  // чтобы линии не выходили за пределы
    painter->setPen(pen);
    const int lineLen = (size - gap);  // длина линий от центра

    // Рисуем 4 линии с зазором от центра
    // Вверх
    painter->drawLine(center.x(), center.y() - gap,
                      center.x(), center.y() - gap - lineLen);

    // Вниз
    painter->drawLine(center.x(), center.y() + gap,
                      center.x(), center.y() + gap + lineLen);

    // Влево
    painter->drawLine(center.x() - gap, center.y(),
                      center.x() - gap - lineLen, center.y());

    // Вправо
    painter->drawLine(center.x() + gap, center.y(),
                      center.x() + gap + lineLen, center.y());

    // Центральная точка (можно настроить размер)
    const int pointRadius = lineWidth / 2;
    painter->setBrush(color);
    painter->drawEllipse(center, pointRadius, pointRadius);
}

void drawRoundedBoxWithInnerLines(QPainter* painter,
                                  const QPoint& center,
                                  int boxSize,
                                  int cornerRadius,
                                  int lineLength,
                                  int lineWidth,
                                  const QColor& color) {
    if (!painter || boxSize <= 0 || lineLength <= 0 || lineWidth <= 0)
        return;

    QPen pen(color);
    pen.setWidth(lineWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    // Вычисляем прямоугольник по центру
    int halfSize = boxSize / 2;
    QRect rect(center.x() - halfSize, center.y() - halfSize, boxSize, boxSize);

    // Рисуем скруглённый квадрат
    painter->drawRoundedRect(rect, cornerRadius, cornerRadius);

    // Центры граней
    QPoint top(center.x(), rect.top());
    QPoint bottom(center.x(), rect.bottom());
    QPoint left(rect.left(), center.y());
    QPoint right(rect.right(), center.y());

    // Функция для рисования короткой линии от стороны внутрь
    auto drawShortLine = [&](const QPoint& from) {
        QPointF dir = center - from;
        double length = std::hypot(dir.x(), dir.y());
        if (length == 0) return;

        QPointF unit = dir / length;
        QPointF end = from + unit * lineLength;

        painter->drawLine(from, end.toPoint());
    };

    drawShortLine(top);
    drawShortLine(bottom);
    drawShortLine(left);
    drawShortLine(right);
}

enum class ArrowMode {
    None,
    BelowLookingUp,
    AboveLookingDown
};

void drawArrowLines(QPainter* painter,
                    const QPoint& center,
                    ArrowMode arrowMode,
                    int diagonalLength,
                    int diagonalOffset,
                    int lineWidth,
                    const QColor& color) {
    if (!painter || arrowMode == ArrowMode::None || diagonalLength <= 0 || diagonalOffset < 0)
        return;

    QPen pen(color);
    pen.setWidth(lineWidth);
    painter->setPen(pen);

    bool drawUp = (arrowMode == ArrowMode::BelowLookingUp);
    int y = drawUp ? center.y() + diagonalOffset
                   : center.y() - diagonalOffset;

    int cx = center.x();
    int delta = static_cast<int>(diagonalLength / std::sqrt(2));

    QPoint startLeft(cx - delta, y);
    QPoint endLeft(cx, y + (drawUp ? -delta : delta));

    QPoint startRight(cx + delta, y);
    QPoint endRight(cx, y + (drawUp ? -delta : delta));

    painter->drawLine(startLeft, endLeft);
    painter->drawLine(startRight, endRight);
}

void drawVerticalRuler(QPainter* painter,
                       const QPoint& topCenter,
                       int totalDivisions,
                       int step,
                       int shortTickLength,
                       int longTickLength,
                       int lineWidth,
                       const QColor& color,
                       bool alignLeft = false,
                       bool drawLabels = false,
                       int labelOffset = 4,
                       const QFont& font = QFont(),
                       std::function<QString(int)> labelFormatter = nullptr,
                       bool drawPointer = false,
                       double pointerPosNormalized = 0.0,
                       QString pointerLabel = "",
                       int pointerSize = 8,
                       bool hollowPointer = false,
                       int pointerOffset = 4,
                       QString rulerTitle = "",
                       int titleOffset = 4,
                       bool drawSetpoint = false,
                       double setpointPosNormalized = 0.0,
                       QString setpointLabel = "",
                       int setpointSize = 8,
                       bool hollowSetpoint = true,
                       int setpointOffset = 4,
                       int setpointLabelHideThreshold = 6 ) {
    if (!painter || totalDivisions <= 0 || step <= 0)
        return;

    QPen pen(color);
    pen.setWidth(lineWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    QFont oldFont = painter->font();
    if (drawLabels) {
        painter->setFont(font);
    }

    int x = topCenter.x();
    int y = topCenter.y();

    for (int i = 0; i < totalDivisions; ++i) {
        int tickLength = (i % 5 == 0) ? longTickLength : shortTickLength;
        int yPos = y + i * step;

        int xStart = x;
        int xEnd = alignLeft ? x - tickLength : x + tickLength;

        painter->drawLine(xStart, yPos, xEnd, yPos);

        if (drawLabels && (i % 5 == 0) && labelFormatter) {
            QString label = labelFormatter(i);
            QFontMetrics fm = painter->fontMetrics();
            painter->setFont(font);
            QRect textRect = fm.boundingRect(label);

            int textX = alignLeft ? xEnd - labelOffset - textRect.width() : xEnd + labelOffset;
            int textY = yPos + textRect.height() / 2 - fm.descent();

            painter->drawText(QPoint(textX, textY), label);
        }
    }

    if (drawLabels) {
        painter->setFont(oldFont);
    }

    if (drawPointer && pointerPosNormalized >= 0.0 && pointerPosNormalized <= 1.0) {
        int rulerHeight = (totalDivisions - 1) * step;
        int pointerY = topCenter.y() + static_cast<int>(pointerPosNormalized * rulerHeight);

        // Стрелка на противоположной стороне от надписей
        bool pointerLeft = !alignLeft;

        int px = topCenter.x();
        int halfHeight = pointerSize / 2;

        // Кончик стрелки у шкалы, основание — снаружи
        int tipX = pointerLeft
                       ? px - longTickLength - pointerOffset
                       : px + longTickLength + pointerOffset;

        int baseX = pointerLeft
                        ? tipX - pointerSize
                        : tipX + pointerSize;

        // Формируем треугольник-стрелку
        QPolygon arrow;
        arrow << QPoint(tipX, pointerY)
              << QPoint(baseX, pointerY - halfHeight)
              << QPoint(baseX, pointerY + halfHeight);

        if (hollowPointer) {
            painter->setBrush(Qt::NoBrush);
            painter->drawPolygon(arrow);
        } else {
            painter->setBrush(color);
            painter->drawPolygon(arrow);
        }

        // Подпись возле основания (вне шкалы)
        QFontMetrics fm = painter->fontMetrics();
        painter->setFont(font);
        QRect textRect = fm.boundingRect(pointerLabel);

        int textX = pointerLeft
                        ? baseX - textRect.width() - 8
                        : baseX + 8;

        QRect labelRect(textX, pointerY - textRect.height() / 2,
                        textRect.width(), textRect.height());
        if(pointerLeft)
            painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, pointerLabel);
        else
            painter->drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, pointerLabel);
    }

    if (drawSetpoint && setpointPosNormalized >= 0.0 && setpointPosNormalized <= 1.0) {
        int rulerHeight = (totalDivisions - 1) * step;
        int setpointY = topCenter.y() + static_cast<int>(setpointPosNormalized * rulerHeight);

        // Сторона та же, что и основной указатель
        bool pointerLeft = !alignLeft;

        int px = topCenter.x();
        int halfHeight = setpointSize / 2;

        int tipX = pointerLeft
                       ? px - longTickLength - setpointOffset
                       : px + longTickLength + setpointOffset;

        int baseX = pointerLeft
                        ? tipX - setpointSize
                        : tipX + setpointSize;

        QPolygon arrow;
        arrow << QPoint(tipX, setpointY)
              << QPoint(baseX, setpointY - halfHeight)
              << QPoint(baseX, setpointY + halfHeight);

        if (hollowSetpoint) {
            pen.setWidth(1);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPolygon(arrow);
        } else {
            painter->setBrush(color);
            painter->drawPolygon(arrow);
        }

        // Вычисляем расстояние по Y между стрелками
        int distanceToMain = std::abs(setpointY - (topCenter.y() + static_cast<int>(pointerPosNormalized * rulerHeight)));

        // Подпись уставки показывается только если указатели далеко
        if (distanceToMain >= setpointLabelHideThreshold && !setpointLabel.isEmpty()) {
            QFontMetrics fm = painter->fontMetrics();
            QRect textRect = fm.boundingRect(setpointLabel);

            int textX = pointerLeft
                            ? baseX - textRect.width() - 8
                            : baseX + 8;

            QRect labelRect(textX, setpointY - textRect.height() / 2,
                            textRect.width(), textRect.height());
            if(pointerLeft)
                painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, setpointLabel);
            else
                painter->drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, setpointLabel);
        }
    }

    if (!rulerTitle.isEmpty()) {
        QFontMetrics fm = painter->fontMetrics();
        QRect titleRect = fm.boundingRect(rulerTitle);

        int textX = alignLeft
                        ? topCenter.x() - longTickLength - titleOffset - titleRect.width()
                        : topCenter.x() + longTickLength + titleOffset;

        int textY = topCenter.y() - step / 2 - 5;

        painter->drawText(QPoint(textX, textY), rulerTitle);
    }
}

void drawHorizontalRuler(QPainter* painter,
                         const QPoint& leftCenter,
                         int totalDivisions,
                         int step,
                         int shortTickLength,
                         int longTickLength,
                         int lineWidth,
                         const QColor& color,
                         bool alignTop = false,
                         bool drawLabels = false,
                         int labelOffset = 4,
                         const QFont& font = QFont(),
                         std::function<QString(int)> labelFormatter = nullptr,
                         bool drawPointer = false,
                         double pointerPosNormalized = 0.0,
                         QString pointerLabel = "",
                         int pointerSize = 8,
                         bool hollowPointer = false,
                         int pointerOffset = 4,
                         QString rulerTitle = "",
                         int titleOffset = 4,
                         bool drawSetpoint = false,
                         double setpointPosNormalized = 0.0,
                         QString setpointLabel = "",
                         int setpointSize = 8,
                         bool hollowSetpoint = true,
                         int setpointOffset = 4,
                         int setpointLabelHideThreshold = 6) {
    if (!painter || totalDivisions <= 0 || step <= 0)
        return;

    QPen pen(color);
    pen.setWidth(lineWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    QFont oldFont = painter->font();
    painter->setFont(font);

    int x = leftCenter.x();
    int y = leftCenter.y();

    for (int i = 0; i < totalDivisions; ++i) {
        int tickLength = (i % 5 == 0) ? longTickLength : shortTickLength;
        int xPos = x + i * step;

        int yStart = y;
        int yEnd = alignTop ? y - tickLength : y + tickLength;

        painter->drawLine(xPos, yStart, xPos, yEnd);

        if (drawLabels && (i % 5 == 0) && labelFormatter) {
            QString label = labelFormatter(i);
            QFontMetrics fm = painter->fontMetrics();
            QRect textRect = fm.boundingRect(label);

            int textX = xPos - textRect.width() / 2;
            int textY = alignTop
                            ? yEnd - labelOffset - textRect.height()
                            : yEnd + labelOffset;

            QRect labelRect(textX, textY, textRect.width(), textRect.height());
            painter->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, label);
        }
    }

    if (drawPointer && pointerPosNormalized >= 0.0 && pointerPosNormalized <= 1.0) {
        int rulerWidth = (totalDivisions - 1) * step;
        int pointerX = leftCenter.x() + static_cast<int>(pointerPosNormalized * rulerWidth);

        bool pointerTop = !alignTop;
        int py = leftCenter.y();
        int halfWidth = pointerSize / 2;

        int tipY = pointerTop
                       ? py - longTickLength - pointerOffset
                       : py + longTickLength + pointerOffset;

        int baseY = pointerTop
                        ? tipY - pointerSize
                        : tipY + pointerSize;

        QPolygon arrow;
        arrow << QPoint(pointerX, tipY)
              << QPoint(pointerX - halfWidth, baseY)
              << QPoint(pointerX + halfWidth, baseY);

        if (hollowPointer) {
            painter->setBrush(Qt::NoBrush);
        } else {
            painter->setBrush(color);
        }
        painter->drawPolygon(arrow);

        QFontMetrics fm = painter->fontMetrics();
        QRect textRect = fm.boundingRect(pointerLabel);

        int textX = pointerX - textRect.width() / 2;
        int textY = pointerTop
                        ? baseY - textRect.height() - 4
                        : baseY + 4;

        QRect labelRect(textX, textY, textRect.width(), textRect.height());
        painter->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, pointerLabel);
    }

    if (drawSetpoint && setpointPosNormalized >= 0.0 && setpointPosNormalized <= 1.0) {
        int rulerWidth = (totalDivisions - 1) * step;
        int setpointX = leftCenter.x() + static_cast<int>(setpointPosNormalized * rulerWidth);

        bool pointerTop = !alignTop;
        int py = leftCenter.y();
        int halfWidth = setpointSize / 2;

        int tipY = pointerTop
                       ? py - longTickLength - setpointOffset
                       : py + longTickLength + setpointOffset;

        int baseY = pointerTop
                        ? tipY - setpointSize
                        : tipY + setpointSize;

        QPolygon arrow;
        arrow << QPoint(setpointX, tipY)
              << QPoint(setpointX - halfWidth, baseY)
              << QPoint(setpointX + halfWidth, baseY);

        if (hollowSetpoint) {
            pen.setWidth(1);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPolygon(arrow);
        } else {
            painter->setBrush(color);
            painter->drawPolygon(arrow);
        }

        // Вычисляем расстояние по X между стрелками
        int distanceToMain = std::abs(setpointX - (leftCenter.x() + static_cast<int>(pointerPosNormalized * rulerWidth)));

        // Подпись уставки показывается только если указатели далеко
        if (distanceToMain >= setpointLabelHideThreshold && !setpointLabel.isEmpty()) {
            QFontMetrics fm = painter->fontMetrics();
            QRect textRect = fm.boundingRect(setpointLabel);

            int textX = setpointX - textRect.width() / 2;
            int textY = pointerTop
                            ? baseY - textRect.height() - 4
                            : baseY + 4;

            QRect labelRect(textX, textY, textRect.width(), textRect.height());
            painter->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, setpointLabel);
        }
    }

    if (!rulerTitle.isEmpty()) {
        QFontMetrics fm = painter->fontMetrics();
        QRect titleRect = fm.boundingRect(rulerTitle);

        int textX = leftCenter.x() - titleRect.width() / 2;
        int textY = alignTop
                        ? leftCenter.y() - longTickLength - titleOffset - titleRect.height()
                        : leftCenter.y() + longTickLength + titleOffset;

        painter->drawText(QPoint(textX, textY), rulerTitle);
    }

    painter->setFont(oldFont);
}

void drawBatteryIcon(QPainter* painter,
                     const QRect& rect,
                     float value,
                     const QColor& borderColor,
                     const QColor& fillColor,
                     int borderWidth,
                     bool drawText = true,
                     bool drawPercentage = true,
                     const QFont& font = QFont(),
                     const QColor& textColor = Qt::black) {
    if (!painter || value < 0.0f || value > 1.0f) return;

    QPen pen(borderColor);
    pen.setWidth(borderWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    // Рисуем корпус батареи
    painter->drawRect(rect);

    // Заполнение уровня
    int fillWidth = static_cast<int>(rect.width() * value);
    QRect fillRect = rect.adjusted(borderWidth / 2, borderWidth / 2, -borderWidth / 2, -borderWidth / 2);
    fillRect.setWidth(fillWidth);
    painter->setBrush(fillColor);
    painter->drawRect(fillRect);

    // Текст
    if (drawText) {
        painter->setFont(font);
        painter->setPen(textColor);
        QString text = drawPercentage ? QString("%1%").arg(static_cast<int>(value * 100)) : QString::number(value * 100);
        painter->drawText(rect, Qt::AlignCenter, text);
    }
}

void drawVerticalSlidingRuler(QPainter* painter,
                              const QPoint& center,
                              int totalDivisions,
                              int step,
                              int shortTickLength,
                              int longTickLength,
                              int lineWidth,
                              const QColor& color,
                              double value,
                              double scaleStep,
                              const QFont& font,
                              std::function<QString(double)> labelFormatter,
                              bool hollowPointer,
                              QString rulerTitle = "",
                              int titleOffset = 4,
                              bool showPointerLabel = false,
                              QString pointerLabel = "",
                              int pointerLabelOffset = 4,
                              int pointerSize = 8,
                              int pointerOffset = 4) {
    if (!painter || totalDivisions <= 0 || step <= 0 || scaleStep <= 0)
        return;

    QPen pen(color);
    pen.setWidth(lineWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    painter->setFont(font);

    int rulerHeight = (totalDivisions - 1) * step;
    int topY = center.y() - rulerHeight / 2;

    // Вычисляем смещение на основе значения
    double offset = std::fmod(value, scaleStep) / scaleStep * step;
    int startLabel = std::floor(value / scaleStep) - (totalDivisions / 2);

    for (int i = 0; i < totalDivisions; ++i) {
        int tickLength = (i % 5 == 0) ? longTickLength : shortTickLength;
        int yPos = topY + i * step + static_cast<int>(offset);

        painter->drawLine(center.x(), yPos, center.x() + tickLength, yPos);

        if (i % 5 == 0) {
            QString label = labelFormatter((startLabel + i) * scaleStep);
            QFontMetrics fm = painter->fontMetrics();
            QRect textRect = fm.boundingRect(label);

            int textX = center.x() + tickLength + 4;
            int textY = yPos + textRect.height() / 2 - fm.descent();

            painter->drawText(QPoint(textX, textY), label);
        }
    }

    // Указатель в центре
    int pointerY = center.y();
    int halfHeight = pointerSize / 2;

    int tipX = center.x() - pointerOffset;
    int baseX = tipX - pointerSize;

    QPolygon arrow;
    arrow << QPoint(tipX, pointerY)
          << QPoint(baseX, pointerY - halfHeight)
          << QPoint(baseX, pointerY + halfHeight);

    if (hollowPointer) {
        painter->setBrush(Qt::NoBrush);
        painter->drawPolygon(arrow);
    } else {
        painter->setBrush(color);
        painter->drawPolygon(arrow);
    }

    if (showPointerLabel) {
        QFontMetrics fm = painter->fontMetrics();
        QRect textRect = fm.boundingRect(pointerLabel);

        int textX = baseX - textRect.width() - pointerLabelOffset;
        QRect labelRect(textX, pointerY - textRect.height() / 2, textRect.width(), textRect.height());
        painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, pointerLabel);
    }

    if (!rulerTitle.isEmpty()) {
        QFontMetrics fm = painter->fontMetrics();
        //QRect titleRect = fm.boundingRect(rulerTitle);

        int textX = center.x() + longTickLength + titleOffset;
        int textY = topY - 5;

        painter->drawText(QPoint(textX, textY), rulerTitle);
    }
}

void drawHorizontalSlidingRuler(QPainter* painter,
                                const QPoint& center,
                                int totalDivisions,
                                int step,
                                int shortTickLength,
                                int longTickLength,
                                int lineWidth,
                                const QColor& color,
                                double value,
                                double scaleStep,
                                const QFont& font,
                                std::function<QString(double)> labelFormatter,
                                bool hollowPointer,
                                int pointerSize,
                                int pointerOffset,
                                QString rulerTitle = "",
                                int titleOffset = 4,
                                bool showPointerLabel = false,
                                QString pointerLabel = "",
                                int pointerLabelOffset = 4) {
    if (!painter || totalDivisions <= 0 || step <= 0 || scaleStep <= 0)
        return;

    QPen pen(color);
    pen.setWidth(lineWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    painter->setFont(font);

    int rulerWidth = (totalDivisions - 1) * step;
    int leftX = center.x() - rulerWidth / 2;

    // Вычисляем смещение на основе значения
    double offset = std::fmod(value, scaleStep) / scaleStep * step;
    int startLabel = std::floor(value / scaleStep) - (totalDivisions / 2);

    for (int i = 0; i < totalDivisions; ++i) {
        int tickLength = (i % 5 == 0) ? longTickLength : shortTickLength;
        int xPos = leftX + i * step + static_cast<int>(offset);

        painter->drawLine(xPos, center.y(), xPos, center.y() + tickLength);

        if (i % 5 == 0) {
            QString label = labelFormatter((startLabel + i) * scaleStep);
            QFontMetrics fm = painter->fontMetrics();
            QRect textRect = fm.boundingRect(label);

            int textX = xPos - textRect.width() / 2;
            int textY = center.y() + tickLength + textRect.height();

            QRect labelRect(textX, textY - textRect.height(), textRect.width(), textRect.height());
            painter->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, label);
        }
    }

    // Указатель в центре
    int pointerX = center.x();
    int halfWidth = pointerSize / 2;

    int tipY = center.y() - pointerOffset;
    int baseY = tipY - pointerSize;

    QPolygon arrow;
    arrow << QPoint(pointerX, tipY)
          << QPoint(pointerX - halfWidth, baseY)
          << QPoint(pointerX + halfWidth, baseY);

    if (hollowPointer) {
        painter->setBrush(Qt::NoBrush);
        painter->drawPolygon(arrow);
    } else {
        painter->setBrush(color);
        painter->drawPolygon(arrow);
    }

    if (showPointerLabel) {
        QFontMetrics fm = painter->fontMetrics();
        QRect textRect = fm.boundingRect(pointerLabel);

        int textX = pointerX - textRect.width() / 2;
        int textY = baseY - pointerLabelOffset - textRect.height();

        QRect labelRect(textX, textY, textRect.width(), textRect.height());
        painter->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, pointerLabel);
    }

    if (!rulerTitle.isEmpty()) {
        QFontMetrics fm = painter->fontMetrics();
        QRect titleRect = fm.boundingRect(rulerTitle);

        int textX = leftX - titleRect.width() / 2 + rulerWidth / 2;
        int textY = center.y() + longTickLength + titleOffset + fm.height();

        painter->drawText(QPoint(textX, textY), rulerTitle);
    }
}

// Новая функция: drawStatic (рисует только статические части)
void OverlayWidget::drawStatic(QPainter* painter, int width, int height) {
    if (!painter) return;

    painter->setRenderHint(QPainter::Antialiasing, true);

    int screenWidth = width;
    int screenHeight = height;

    QColor defaultColor(Qt::white);

    QFont labelFont("Consolas", 8, QFont::Bold);

    QPoint center(screenWidth / 2, screenHeight / 2);

    // Статический крест
    int crosshairSize = 80;
    int crosshairLineWidth = 2;
    int crosshairGap = 10;
    drawCrosshair(painter, center, crosshairSize, crosshairLineWidth, crosshairGap, defaultColor);

    // Статическая линейка для угла камеры (без указателя и уставки)
    int camAngleRulerNumNotches = 11;
    int camAngleRulerHeight = 150;
    int camAngleRulerNotchSpacing = camAngleRulerHeight / (camAngleRulerNumNotches - 1);
    QPoint camAngleRulerPos(80, screenHeight - camAngleRulerHeight - camAngleRulerHeight / 3);
    int camAngleRulerShortNotch = 4;
    int camAngleRulerLongNotch = 9;
    int camAngleRullerLineWidth = 2;
    QColor camAngleRulerColor = defaultColor;
    bool camAngleRulerLeft = true;
    bool camAngleRulerDrawLabels = true;
    int camAngleRulerLabelsOffset = 4;
    QString camAngleRuleTitle = "Угол камеры";
    int camAngleRulerTitleOffset = -20;

    drawVerticalRuler(painter,
                      camAngleRulerPos,
                      camAngleRulerNumNotches,
                      camAngleRulerNotchSpacing,
                      camAngleRulerShortNotch,
                      camAngleRulerLongNotch,
                      camAngleRullerLineWidth,
                      camAngleRulerColor,
                      camAngleRulerLeft,
                      camAngleRulerDrawLabels,
                      camAngleRulerLabelsOffset,
                      labelFont,
                      [](int i) { return QString::number(-(i * 18 - 90)); },
                      false,  // Без указателя
                      0.0,
                      "",
                      8,
                      true,
                      -5,
                      camAngleRuleTitle,
                      camAngleRulerTitleOffset,
                      false);  // Без уставки

    // Статическая вертикальная линейка дифферента (без указателя и уставки)
    int pitchRulerNumNotches = 31;
    int pitchRulerHeight = screenHeight / 8 * 3;
    int pitchRulerNotchSpacing = pitchRulerHeight / (pitchRulerNumNotches - 1);
    int pitchRillerY = screenHeight / 2 - pitchRulerNotchSpacing * (pitchRulerNumNotches - 1) / 2;
    QPoint pitchRulerPos(screenWidth / 3, pitchRillerY);
    int pitchRulerShortNotch = 5;
    int pitchRulerLongNotch = 12;
    int pitchRullerLineWidth = 2;
    QColor pitchRulerColor = defaultColor;
    bool pitchRulerLeft = true;
    bool pitchRulerDrawLabels = true;
    int pitchRulerLabelsOffset = 4;
    QString pitchRuleTitle = "Дифферент";
    int pitchRulerTitleOffset = -20;

    drawVerticalRuler(painter,
                      pitchRulerPos,
                      pitchRulerNumNotches,
                      pitchRulerNotchSpacing,
                      pitchRulerShortNotch,
                      pitchRulerLongNotch,
                      pitchRullerLineWidth,
                      pitchRulerColor,
                      pitchRulerLeft,
                      pitchRulerDrawLabels,
                      pitchRulerLabelsOffset,
                      labelFont,
                      [](int i) { return QString::number(-(i * 6 - 90)); },
                      false,  // Без указателя
                      0.0,
                      "",
                      8,
                      true,
                      -5,
                      pitchRuleTitle,
                      pitchRulerTitleOffset,
                      false);  // Без уставки

    // Статическая горизонтальная линейка крена (без указателя и уставки)
    int rollRulerNumNotches = 31;
    int rollRulerWidth = pitchRulerHeight;
    int rollRulerNotchSpacing = rollRulerWidth / (rollRulerNumNotches - 1);
    QPoint rollRulerPos(screenWidth / 2 - rollRulerNotchSpacing * (rollRulerNumNotches - 1) / 2, pitchRillerY + pitchRulerHeight + pitchRulerHeight/4);
    int rollRulerShortNotch = 5;
    int rollRulerLongNotch = 12;
    int rollRullerLineWidth = 2;
    QColor rollRulerColor = defaultColor;
    bool rollRulerBot = false;
    bool rollRulerDrawLabels = true;
    int rollRulerLabelsOffset = 4;
    QString rollRuleTitle = "Крен";
    int rollRulerTitleOffset = 20;

    drawHorizontalRuler(painter,
                        rollRulerPos,
                        rollRulerNumNotches,
                        rollRulerNotchSpacing,
                        rollRulerShortNotch,
                        rollRulerLongNotch,
                        rollRullerLineWidth,
                        rollRulerColor,
                        rollRulerBot,
                        rollRulerDrawLabels,
                        rollRulerLabelsOffset,
                        labelFont,
                        [](int i) { return QString::number((i * 6 - 90)); },
                        false,  // Без указателя
                        0.0,
                        "",
                        8,
                        true,
                        -5,
                        rollRuleTitle,
                        rollRulerTitleOffset,
                        false);  // Без уставки
}

// Новая функция: drawDynamic (рисует только изменяющиеся части)
void OverlayWidget::drawDynamic(QPainter* painter, int width, int height) {
    if (!painter) return;

    painter->setRenderHint(QPainter::Antialiasing, true);

    int screenWidth = width;
    int screenHeight = height;

    QColor defaultColor(Qt::white);

    QFont labelFont("Consolas", 8, QFont::Bold);

    QPoint center(screenWidth / 2, screenHeight / 2);

    // Динамическая часть dirRect и arrows (зависит от ocamAngle)
    //const int CameraVerticalAngle = 60;

    ArrowMode camLook = ArrowMode::None;
    //int pixInDeg = screenHeight / CameraVerticalAngle;
    int camY = center.y(); //+ ocamAngle * pixInDeg;
    if(camY <= (screenHeight / 5)){
        camY = screenHeight / 5;
        camLook = ArrowMode::BelowLookingUp;
    }
    if(camY >= (screenHeight - (screenHeight / 5))){
        camY = screenHeight - (screenHeight / 5);
        camLook = ArrowMode::AboveLookingDown;
    }

    QPoint dirRectCenter(center.x(),  camY);
    int dirRectSize = 80 * 2 + 20;  // crosshairSize=80 из static
    int dirRectRadis = 10;
    int dirRectLineLen = (dirRectSize - 80 * 2) / 2 - 5;
    int dirRectLineWidth = 1;
    QColor dirRectColor = defaultColor;
    QColor dirArrowsColor = defaultColor;
    int arrowsLenghts = 20;
    int arrowsOffset = 0;

    if(camLook == ArrowMode::None)
        drawRoundedBoxWithInnerLines(painter, dirRectCenter, dirRectSize, dirRectRadis, dirRectLineLen, dirRectLineWidth, dirRectColor);
    else
        drawArrowLines(painter, dirRectCenter, camLook, arrowsLenghts, arrowsOffset, dirRectLineWidth, dirArrowsColor);

    // Динамическая часть линейки угла камеры (только указатель)
    int camAngleRulerNumNotches = 11;
    int camAngleRulerHeight = 150;
    int camAngleRulerNotchSpacing = camAngleRulerHeight / (camAngleRulerNumNotches - 1);
    QPoint camAngleRulerPos(80, screenHeight - camAngleRulerHeight - camAngleRulerHeight / 3);
    int camAngleRulerShortNotch = 4;
    int camAngleRulerLongNotch = 9;
    int camAngleRullerLineWidth = 2;
    QColor camAngleRulerColor = defaultColor;
    bool camAngleRulerLeft = true;
    int camAngleRulerLabelsOffset = 4;
    bool camAngleRulerDrawPoimter = true;
    int camAngleRulerPointerSize = 8;
    bool camAngleRulerPointerHollow = true;
    int camAngleRulerPointerOffset = -5;
    double camAngleRulerPointerPos = (double(90.0f - ocamAngle) / 180.0f);
    QString camAngleRulerPointerValue = QString::number(std::round(ocamAngle));

    drawVerticalRuler(painter,
                      camAngleRulerPos,
                      camAngleRulerNumNotches,
                      camAngleRulerNotchSpacing,
                      camAngleRulerShortNotch,
                      camAngleRulerLongNotch,
                      camAngleRullerLineWidth,
                      camAngleRulerColor,
                      camAngleRulerLeft,
                      false,  // Без labels (уже в static)
                      camAngleRulerLabelsOffset,
                      labelFont,
                      [](int i) { return QString::number(-(i * 18 - 90)); },
                      camAngleRulerDrawPoimter,
                      camAngleRulerPointerPos,
                      camAngleRulerPointerValue,
                      camAngleRulerPointerSize,
                      camAngleRulerPointerHollow,
                      camAngleRulerPointerOffset,
                      "",  // Без title (в static)
                      0,
                      false);  // Без setpoint

    // Динамическая часть линейки дифферента (указатель и setpoint)
    int pitchRulerNumNotches = 31;
    int pitchRulerHeight = screenHeight / 8 * 3;
    int pitchRulerNotchSpacing = pitchRulerHeight / (pitchRulerNumNotches - 1);
    int pitchRillerY = screenHeight / 2 - pitchRulerNotchSpacing * (pitchRulerNumNotches - 1) / 2;
    QPoint pitchRulerPos(screenWidth / 3, pitchRillerY);
    int pitchRulerShortNotch = 5;
    int pitchRulerLongNotch = 12;
    int pitchRullerLineWidth = 2;
    QColor pitchRulerColor = defaultColor;
    bool pitchRulerLeft = true;
    int pitchRulerLabelsOffset = 4;
    bool pitchRulerDrawPoimter = true;
    int pitchRulerPointerSize = 8;
    bool pitchRulerPointerHollow = true;
    int pitchRulerPointerOffset = -5;
    double pitchRulerPointerPos = (double(90.0f - oPitch) / 180.0f);
    QString pitchRulerPointerValue = QString::number(std::round(oPitch));
    bool pitchRulerDrawSetpoint = ostabPitch;
    double pitchRulerSetpointPos = (double(90.0f - oPitchSetpoint) / 180.0f);
    QString pitchRulerSetpointValue = QString::number(std::round(oPitchSetpoint));
    int pitchRulerSetpointSize = pitchRulerPointerSize/2;
    bool pitchRulerSetpointHollow = false;
    int pitchRulerSetpointOffset = pitchRulerPointerOffset + pitchRulerSetpointSize/2;
    int pitchRulerSetpointHideTreshold = 12;

    drawVerticalRuler(painter,
                      pitchRulerPos,
                      pitchRulerNumNotches,
                      pitchRulerNotchSpacing,
                      pitchRulerShortNotch,
                      pitchRulerLongNotch,
                      pitchRullerLineWidth,
                      pitchRulerColor,
                      pitchRulerLeft,
                      false,  // Без labels
                      pitchRulerLabelsOffset,
                      labelFont,
                      [](int i) { return QString::number(-(i * 6 - 90)); },
                      pitchRulerDrawPoimter,
                      pitchRulerPointerPos,
                      pitchRulerPointerValue,
                      pitchRulerPointerSize,
                      pitchRulerPointerHollow,
                      pitchRulerPointerOffset,
                      "",  // Без title
                      0,
                      pitchRulerDrawSetpoint,
                      pitchRulerSetpointPos,
                      pitchRulerSetpointValue,
                      pitchRulerSetpointSize,
                      pitchRulerSetpointHollow,
                      pitchRulerSetpointOffset,
                      pitchRulerSetpointHideTreshold);

    // Динамическая часть линейки крена (указатель и setpoint)
    int rollRulerNumNotches = 31;
    int rollRulerWidth = pitchRulerHeight;
    int rollRulerNotchSpacing = rollRulerWidth / (rollRulerNumNotches - 1);
    QPoint rollRulerPos(screenWidth / 2 - rollRulerNotchSpacing * (rollRulerNumNotches - 1) / 2, pitchRillerY + pitchRulerHeight + pitchRulerHeight/4);
    int rollRulerShortNotch = 5;
    int rollRulerLongNotch = 12;
    int rollRullerLineWidth = 2;
    QColor rollRulerColor = defaultColor;
    bool rollRulerBot = false;
    int rollRulerLabelsOffset = 4;
    bool rollRulerDrawPoimter = true;
    int rollRulerPointerSize = 8;
    bool rollRulerPointerHollow = true;
    int rollRulerPointerOffset = -5;
    double rollRulerPointerPos = (double(90.0f + oRoll) / 180.0f);
    QString rollRulerPointerValue = QString::number(std::round(oRoll));
    bool rollRulerDrawSetpoint = ostabRoll;
    double rollRulerSetpointPos = (double(90.0f + oRollSetpoint) / 180.0f);
    QString rollRulerSetpointValue = QString::number(std::round(oRollSetpoint));
    int rollRulerSetpointSize = rollRulerPointerSize/2;
    bool rollRulerSetpointHollow = false;
    int rollRulerSetpointOffset = rollRulerPointerOffset + rollRulerSetpointSize/2;
    int rollRulerSetpointHideTreshold = 12;

    drawHorizontalRuler(painter,
                        rollRulerPos,
                        rollRulerNumNotches,
                        rollRulerNotchSpacing,
                        rollRulerShortNotch,
                        rollRulerLongNotch,
                        rollRullerLineWidth,
                        rollRulerColor,
                        rollRulerBot,
                        false,  // Без labels
                        rollRulerLabelsOffset,
                        labelFont,
                        [](int i) { return QString::number((i * 6 - 90)); },
                        rollRulerDrawPoimter,
                        rollRulerPointerPos,
                        rollRulerPointerValue,
                        rollRulerPointerSize,
                        rollRulerPointerHollow,
                        rollRulerPointerOffset,
                        "",  // Без title
                        0,
                        rollRulerDrawSetpoint,
                        rollRulerSetpointPos,
                        rollRulerSetpointValue,
                        rollRulerSetpointSize,
                        rollRulerSetpointHollow,
                        rollRulerSetpointOffset,
                        rollRulerSetpointHideTreshold);

    // Скользящая линейка глубины (полностью динамическая)
    int depthRulerNumNotches = 21;
    int depthRulerHeight = pitchRulerHeight;
    int depthRulerNotchSpacing = depthRulerHeight / (depthRulerNumNotches-1);
    QPoint depthRulerPosition(screenWidth/3*2, screenHeight / 2);
    int depthRulerNotchLong = 12;
    int depthRulerNotchShort = 5;
    int depthRulerLineWidth = 2;
    QColor depthRulerColor = defaultColor;
    double depthRulerValue = oDepth;
    int depthRulerStep = 1;
    bool depthRulerPointerHolow = !ostabDepth;
    QString depthRulerTitle = "Глубина";
    int depthRulerTitleOffset = -20;  // pitchRulerTitleOffset из оригинала
    bool depthRulerShowPointerLabel = true;
    QString depthRulerValueName = QString::number(depthRulerValue);
    int depthRulerPointerSize = 8;  // pitchRulerPointerSize
    int depthRulerPointerOffset = 5;  // -pitchRulerPointerOffset из оригинала (было -(-5)=5?)
    int depthRulerPointerLabelOffset = depthRulerPointerOffset + 14;

    drawVerticalSlidingRuler(painter,
                             depthRulerPosition,
                             depthRulerNumNotches,
                             depthRulerNotchSpacing,
                             depthRulerNotchShort,
                             depthRulerNotchLong,
                             depthRulerLineWidth,
                             depthRulerColor,
                             depthRulerValue,
                             depthRulerStep,
                             labelFont,
                             [](double v) { return QString::number(v); },
                             depthRulerPointerHolow,
                             depthRulerTitle,
                             depthRulerTitleOffset,
                             depthRulerShowPointerLabel,
                             depthRulerValueName,
                             depthRulerPointerLabelOffset,
                             depthRulerPointerSize,
                             depthRulerPointerOffset);

    // Скользящая горизонтальная линейка для курса (полностью динамическая)
    int yawRulerNumNotches = 61;
    int yawRulerWidth = screenWidth/8*6;
    int yawRulerNotchSpacing = yawRulerWidth / (yawRulerNumNotches-1);
    QPoint yawRulerPosition(screenWidth/2, screenHeight / 12);
    int yawRulerNotchLong = 16;
    int yawRulerNotchShort = 10;
    int yawRulerLineWidth = 2;
    QColor yawRulerColor = defaultColor;
    double yawRulerValue = fmod(oYaw + 360.0, 360.0);
    int yawRulerStep = 1;
    bool yawRulerPointerHolow = !ostabYaw;
    QString yawRulerTitle = "Курс";
    int yawRulerTitleOffset = -20;  // pitchRulerTitleOffset
    bool yawRulerShowPointerLabel = true;
    QString yawRulerValueName = QString::number(std::round(yawRulerValue));
    int yawRulerPointerSize = 8;
    int yawRulerPointerOffset = 5;
    int yawRulerPointerLabelOffset = yawRulerPointerOffset;

    drawHorizontalSlidingRuler(painter,
                               yawRulerPosition,
                               yawRulerNumNotches,
                               yawRulerNotchSpacing,
                               yawRulerNotchShort,
                               yawRulerNotchLong,
                               yawRulerLineWidth,
                               yawRulerColor,
                               yawRulerValue,
                               yawRulerStep,
                               labelFont,
                               [](double angle) {
                                   int wrapped = static_cast<int>(std::round(angle)) % 360;
                                   if (wrapped < 0) wrapped += 360;
                                   return QString::number(wrapped) + "°";
                               },
                               yawRulerPointerHolow,
                               yawRulerPointerSize,
                               yawRulerPointerOffset,
                               yawRulerTitle,
                               yawRulerTitleOffset,
                               yawRulerShowPointerLabel,
                               yawRulerValueName,
                               yawRulerPointerLabelOffset);

    // Заряд батареи (динамическая)
    QRect batteryArea(screenWidth / 30, screenHeight / 12 - 26, 55, 26);
    float batteryValue = oBatLevel;
    QColor batteryColor(Qt::green);
    if(batteryValue < 0.6)
        batteryColor = Qt::yellow;
    if(batteryValue < 0.3)
        batteryColor = Qt::red;
    int batteryBorderWidth = 2;
    QFont batteryFont("Consolas", 12, QFont::Bold);
    QColor batteryTextColor(Qt::darkGray);
    drawBatteryIcon(painter,
                    batteryArea,
                    batteryValue,
                    defaultColor,
                    batteryColor,
                    batteryBorderWidth,
                    true,
                    true,
                    batteryFont,
                    batteryTextColor);

    // Счетчик оборотов (динамический)
    QRect revolutionCounterRect(screenWidth / 30, screenHeight/12 + 20, 120, 20);
    QFont revFont("Consolas", 12, QFont::Bold);
    QColor revColor = defaultColor;
    painter->setFont(revFont);
    painter->setPen(revColor);
    painter->drawText(revolutionCounterRect, Qt::AlignLeft, "Обороты: " + QString::number(revolutionCount));

    // Состояние светильников (динамическое)
    QRect lightsRect(screenWidth / 30, screenHeight/12 + 40, 220, 20);
    QFont lightsFont("Consolas", 12, QFont::Bold);
    QColor lightsColor = defaultColor;
    painter->setFont(lightsFont);
    painter->setPen(lightsColor);
    if(oLightsState)
        painter->drawText(lightsRect, Qt::AlignLeft, "Освещение: вкл.");
    else
        painter->drawText(lightsRect, Qt::AlignLeft, "Освещение: выкл.");
}

// Модифицированный paintEvent: использует кэш + dynamic
void OverlayWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    if (!staticOverlay.isNull()) {
        painter.drawPixmap(0, 0, staticOverlay);
    }
    drawDynamic(&painter, width(), height());
    QWidget::paintEvent(event);
}

void OverlayWidget::pushOverlayToQueue(int width, int height) {
    if (!m_overlayInfo) {
        qDebug() << "Warning: OverlayFrameInfo not provided, skipping queue push.";
        return;
    }

    QImage frame;

    {
        QMutexLocker locker(m_overlayInfo->mutex);
        if (!m_overlayInfo->originalQueue.empty()) {
            frame = m_overlayInfo->originalQueue.front();
            m_overlayInfo->originalQueue.pop_front();
        }
    }

    if (frame.isNull()) { return; }

    // Проверяем формат QImage
    QImage overlayImage = frame.copy();


    QPainter painter(&overlayImage);
    if (!painter.isActive()) {
        qDebug() << "Error: QPainter failed to initialize on QImage.";
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    drawStatic(&painter, 1440, 1080); // не работает как надо, кадры не поступают
    drawDynamic(&painter, 1440, 1080);

    // Конвертируем QImage в cv::Mat
    cv::Mat matWithOverlay(overlayImage.height(), overlayImage.width(), CV_8UC3, overlayImage.bits(), overlayImage.bytesPerLine());

    {
        QMutexLocker locker(m_overlayInfo->mutex);
        m_overlayInfo->overlayQueue.push_back(matWithOverlay.clone());
        if (m_overlayInfo->overlayQueue.size() > m_overlayInfo->maxQueueSize) {
            m_overlayInfo->overlayQueue.pop_front();

        }
    }
}

float constrainff(const float value, const float lower_limit, const float upper_limit){
    if(value>upper_limit) return upper_limit;
    if(value<lower_limit) return lower_limit;
    return value;
}

void OverlayWidget::telemetryUpdate(TelemetryPacket& telemetry){
    oPitch = telemetry.pitch;
    oRoll = telemetry.roll;
    oYaw = telemetry.yaw;
    countRevolutions();
    oDepth = telemetry.depth;
    oPitchSetpoint = constrainff(telemetry.pitchSP, -90, 90);
    oRollSetpoint = constrainff(telemetry.rollSP, -90, 90);
    oBatLevel = constrainff(telemetry.batCharge/100.0f, 0.0f, 1.0f);
}

void OverlayWidget::controlsUpdate(const bool& stabEnabled,
                                   const bool& stabRoll,
                                   const bool& stabPitch,
                                   const bool& stabYaw,
                                   const bool& stabDepth,
                                   const bool& masterFlag,
                                   const float& powerLimit,
                                   const float& camAngle,
                                   const bool& lightsState){
    ostabEnabled = stabEnabled;
    ostabRoll = stabRoll;
    ostabPitch = stabPitch;
    ostabYaw = stabYaw;
    ostabDepth = stabDepth;
    omasterFlag = masterFlag;
    opowerLimit = powerLimit;
    ocamAngle = constrainff(camAngle, -90, 90);
    oLightsState = lightsState;
}

void OverlayWidget::updateOverlay(){
    emit requestOverlayDataUpdate();

    int w = parentWidget->width();
    int h = parentWidget->height();
    this->setGeometry(0, 0, w, h);

    // Проверяем и обновляем кэш, если размер изменился или кэш невалиден
    if (!cacheValid || staticOverlay.size() != QSize(w, h)) {
        staticOverlay = QPixmap(w, h);
        staticOverlay.fill(Qt::transparent);
        QPainter cachePainter(&staticOverlay);
        drawStatic(&cachePainter, w, h);
        cacheValid = true;
    }

    this->update();
}

void OverlayWidget::countRevolutions(){
    float delta = oYaw - prevYaw;
    if (delta > 180.0)
        revolutionCount--;
    else if (delta < -180.0)
        revolutionCount++;
    prevYaw = oYaw;
}

void OverlayWidget::pushOverlayToQueueSlot() {
    pushOverlayToQueue(parentWidget->width(), parentWidget->height());
}
