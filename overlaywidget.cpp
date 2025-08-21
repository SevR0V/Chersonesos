#include "overlaywidget.h"
#include <QScreen>
#include <QWindow>

OverlayWidget::OverlayWidget(QWidget *parent) : QWidget(parent)
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

        // int textY = pointerY + textRect.height() / 2 - fm.descent();

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

            // int textY = setpointY + textRect.height() / 2 - fm.descent();
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
            painter->setBrush(Qt::NoBrush);
        } else {
            painter->setBrush(color);
        }
        painter->drawPolygon(arrow);

        int distanceToMain = std::abs(setpointX - (leftCenter.x() + static_cast<int>(pointerPosNormalized * (totalDivisions - 1) * step)));

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

        int rulerWidth = (totalDivisions - 1) * step;
        int rulerCenterX = leftCenter.x() + rulerWidth / 2;

        int textX = rulerCenterX - titleRect.width() / 2;
        int textY = alignTop
                        ? leftCenter.y() - longTickLength - titleOffset - titleRect.height()
                        : leftCenter.y() + longTickLength + titleOffset;

        QRect titleRectAligned(textX, textY, titleRect.width(), titleRect.height());
        painter->drawText(titleRectAligned, Qt::AlignHCenter | Qt::AlignTop, rulerTitle);
    }

    painter->setFont(oldFont);
}

void drawVerticalSlidingRuler(QPainter* painter,
                              const QPoint& center,
                              int visibleDivisions,
                              int step,
                              int shortTickLength,
                              int longTickLength,
                              int lineWidth,
                              const QColor& color,
                              double currentValue,
                              double divisionStepValue = 1.0,
                              const QFont& font = QFont(),
                              std::function<QString(double)> labelFormatter = nullptr,
                              bool hollowPointer = false,
                              QString rulerTitle = "",
                              int titleOffset = 4,
                              bool showCurrentLabel = false,
                              QString currentLabel = "",
                              int currentLabelOffset = 6,
                              int pointerSize = 6,
                              int pointerOffset = 4) {
    if (!painter || visibleDivisions <= 0 || step <= 0 || divisionStepValue <= 0.0)
        return;

    QPen pen(color);
    pen.setWidth(lineWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->setFont(font);

    QFontMetrics fm = painter->fontMetrics();
    int cx = center.x();
    int cy = center.y();

    // Диапазон значений, которые попадают в шкалу
    double halfRange = (visibleDivisions / 2.0) * divisionStepValue;
    double minValue = currentValue - halfRange;
    double maxValue = currentValue + halfRange;

    // Начинаем с ближайшего меньшего "основного" деления
    double startValue = std::floor(minValue / divisionStepValue) * divisionStepValue;

    for (double val = startValue; val <= maxValue + divisionStepValue; val += divisionStepValue) {
        double delta = val - currentValue;
        int yPos = cy + static_cast<int>(delta / divisionStepValue * step);

        // Пропустить, если далеко (за экраном)
        if (std::abs(yPos - cy) > (visibleDivisions * step / 2 + step))
            continue;

        bool isMajor = std::fmod(std::fabs(val), divisionStepValue * 5.0) < 1e-6;
        int tickLength = isMajor ? longTickLength : shortTickLength;

        int xStart = cx;
        int xEnd = cx + tickLength;

        painter->drawLine(xStart, yPos, xEnd, yPos);

        if (isMajor && labelFormatter) {
            QString label = labelFormatter(val);
            QRect textRect = fm.boundingRect(label);

            int textX = xEnd + 4;
            int textY = yPos + textRect.height() / 2 - fm.descent();

            painter->drawText(QPoint(textX, textY), label);
        }
    }

    // Указатель (треугольник вправо)
    int halfHeight = pointerSize / 2;
    int tipX = cx - pointerOffset;          // кончик стрелки чуть левее шкалы
    int baseX = tipX - pointerSize;         // основание стрелки ещё левее

    QPolygon arrow;
    arrow << QPoint(tipX, cy)
          << QPoint(baseX, cy - halfHeight)
          << QPoint(baseX, cy + halfHeight);

    if (hollowPointer) {
        painter->setBrush(Qt::NoBrush);
    } else {
        painter->setBrush(color);
    }
    painter->drawPolygon(arrow);

    // Подпись текущего значения
    if (showCurrentLabel && !currentLabel.isEmpty()) {
        QRect textRect = fm.boundingRect(currentLabel);
        int textX = cx - textRect.width() - currentLabelOffset;
        int textY = cy + textRect.height() / 2 - fm.descent();

        painter->drawText(QPoint(textX, textY), currentLabel);
    }

    // Заголовок
    if (!rulerTitle.isEmpty()) {
        QRect titleRect = fm.boundingRect(rulerTitle);
        int textX = cx + longTickLength + titleOffset;
        int textY = cy - visibleDivisions * step / 2 - titleRect.height() - 2;

        painter->drawText(QPoint(textX, textY), rulerTitle);
    }
}

void drawHorizontalSlidingRuler(QPainter* painter,
                                const QPoint& center,
                                int visibleDivisions,
                                int step,
                                int shortTickLength,
                                int longTickLength,
                                int lineWidth,
                                const QColor& color,
                                double currentValue,
                                double divisionStepValue = 1.0,
                                const QFont& font = QFont(),
                                std::function<QString(double)> labelFormatter = nullptr,
                                bool hollowPointer = false,
                                int pointerSize = 6,
                                int pointerOffset = 4,
                                QString rulerTitle = "",
                                int titleOffset = 4,
                                bool showCurrentLabel = false,
                                QString currentLabel = "",
                                int currentLabelOffset = 6) {
    if (!painter || visibleDivisions <= 0 || step <= 0 || divisionStepValue <= 0.0)
        return;

    QPen pen(color);
    pen.setWidth(lineWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->setFont(font);

    QFontMetrics fm = painter->fontMetrics();

    int cx = center.x();
    int cy = center.y();

    double halfRange = (visibleDivisions / 2.0) * divisionStepValue;
    double minValue = currentValue - halfRange;
    double maxValue = currentValue + halfRange;

    double startValue = std::floor(minValue / divisionStepValue) * divisionStepValue;

    for (double val = startValue; val <= maxValue + divisionStepValue; val += divisionStepValue) {
        double delta = val - currentValue;
        int xPos = cx + static_cast<int>(delta / divisionStepValue * step);

        if (std::abs(xPos - cx) > (visibleDivisions * step / 2 + step))
            continue;

        bool isMajor = std::fmod(std::fabs(val), divisionStepValue * 5.0) < 1e-6;
        int tickLength = isMajor ? longTickLength : shortTickLength;

        int yStart = cy;
        int yEnd = cy - tickLength;

        painter->drawLine(xPos, yStart, xPos, yEnd);

        if (isMajor && labelFormatter) {
            QString label = labelFormatter(val);
            QRect textRect = fm.boundingRect(label);

            int textX = xPos - textRect.width() / 2;
            int textY = yEnd - 4;

            painter->drawText(QPoint(textX, textY), label);
        }
    }

    // Указатель (треугольник вверх)
    int halfWidth = pointerSize / 2;
    int tipY = cy + pointerOffset;
    int baseY = tipY + pointerSize;

    QPolygon arrow;
    arrow << QPoint(cx, tipY)
          << QPoint(cx - halfWidth, baseY)
          << QPoint(cx + halfWidth, baseY);

    if (hollowPointer)
        painter->setBrush(Qt::NoBrush);
    else
        painter->setBrush(color);

    painter->drawPolygon(arrow);

    // Подпись текущего значения
    if (showCurrentLabel && !currentLabel.isEmpty()) {
        QRect textRect = fm.boundingRect(currentLabel);
        int textX = cx - textRect.width() / 2;
        int textY = baseY + textRect.height() + currentLabelOffset;

        painter->drawText(QPoint(textX, textY), currentLabel);
    }

    // Заголовок шкалы сверху
    if (!rulerTitle.isEmpty()) {
        QRect titleRect = fm.boundingRect(rulerTitle);
        int textX = cx - titleRect.width() / 2;
        int textY = cy - visibleDivisions * step / 2 - titleRect.height() - titleOffset;

        painter->drawText(QPoint(textX, textY), rulerTitle);
    }
}

void drawBatteryIcon(QPainter* painter,
                     const QRect& rect,
                     double level,                             // 0.0 – 1.0
                     const QColor& borderColor = Qt::black,
                     const QColor& fillColor = Qt::green,
                     int borderWidth = 2,
                     bool showCap = true,
                     bool showPercent = true,
                     const QFont& percentFont = QFont(),
                     const QColor& textColor = Qt::black) {
    if (!painter || level < 0.0 || level > 1.0)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen pen(borderColor, borderWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    QRect bodyRect = rect;

    // Крышка
    int capWidth = showCap ? rect.width() / 10 : 0;
    int capHeight = rect.height() / 3;

    if (showCap) {
        QRect cap(rect.right() + 1, rect.center().y() - capHeight / 2, capWidth, capHeight);
        painter->setBrush(borderColor);
        painter->drawRect(cap);
    }

    // Контур батареи
    bodyRect.setWidth(bodyRect.width() - capWidth - 2);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(bodyRect);

    // Внутреннее заполнение
    int margin = borderWidth + 1;
    QRect fillRect = bodyRect.adjusted(margin, margin, -margin, -margin);
    int fillWidth = static_cast<int>(fillRect.width() * std::clamp(level, 0.0, 1.0));
    QRect chargeRect = QRect(fillRect.left(), fillRect.top(), fillWidth, fillRect.height());

    painter->setBrush(fillColor);
    painter->setPen(Qt::NoPen);
    painter->drawRect(chargeRect);

    // Текст процента
    if (showPercent) {
        painter->setFont(percentFont);
        painter->setPen(textColor);
        QString percentText = QString::number(static_cast<int>(level * 100)) + "%";

        painter->drawText(bodyRect, Qt::AlignCenter, percentText);
    }

    painter->restore();
}

void OverlayWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont labelFont("Consolas", 10);
    QFont bigLabelFont("Consolas", 14);
    int screenWidth = width();

    int screenHeight = height();
    int CameraVerticalAngle = 56;

    QColor defaultColor(Qt::lightGray);

    int centerX = screenWidth/2;
    int centerY = screenHeight/2;
    QPoint center(centerX, centerY);

    //Перекрестие - центр камеры
    int crosshairSize = 20;
    int crosshairGap = crosshairSize/3;
    int crosshairLineWidth = 1;
    QColor crosshairColor = defaultColor;

    drawCrosshair(&painter, center, crosshairSize, crosshairLineWidth, crosshairGap, crosshairColor);

    //Квадрат - указатель направления аппарата
    //Квадрат переключается на стрелку, если камера отклонена так, что направление аппарата вне видимости (помогает с ориентированием)
    ArrowMode camLook = ArrowMode::None;
    int pixInDeg = screenHeight / CameraVerticalAngle;
    int camY = centerY; //+ ocamAngle * pixInDeg;
    if(camY <= (screenHeight / 5)){
        camY = screenHeight / 5;
        camLook = ArrowMode::BelowLookingUp;
    }
    if(camY >= (screenHeight - (screenHeight / 5))){
        camY = screenHeight - (screenHeight / 5);
        camLook = ArrowMode::AboveLookingDown;
    }

    QPoint dirRectCenter(centerX,  camY);
    int dirRectSize = crosshairSize * 2 + 20;
    int dirRectRadis = 10;
    int dirRectLineLen = (dirRectSize - crosshairSize * 2) / 2 - 5;
    int dirRectLineWidth = 1;
    QColor dirRectColor = defaultColor;
    QColor dirArrowsColor = defaultColor;
    int arrowsLenghts = 20;
    int arrowsOffset = 0;

    if(camLook == ArrowMode::None)
        drawRoundedBoxWithInnerLines(&painter, dirRectCenter, dirRectSize, dirRectRadis, dirRectLineLen, dirRectLineWidth, dirRectColor);
    else
        drawArrowLines(&painter, dirRectCenter, camLook, arrowsLenghts, arrowsOffset, dirRectLineWidth, dirArrowsColor);

    //статическая линейка для угла камеры
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
    bool camAngleRulerDrawPoimter = true;
    int camAngleRulerPointerSize = 8;
    bool camAngleRulerPointerHollow = true;
    int camAngleRulerPointerOffset = -5;
    double camAngleRulerPointerPos = (double(90.0f - ocamAngle) / 180.0f);
    QString camAngleRulerPointerValue = QString::number(std::round(ocamAngle));
    QString camAngleRuleTitle = "Угол камеры";
    int camAngleRulerTitleOffset = -20;

    drawVerticalRuler(&painter,
                      camAngleRulerPos,
                      camAngleRulerNumNotches,        // 30 делений
                      camAngleRulerNotchSpacing,        // расстояние между рисками = 10 пикселей
                      camAngleRulerShortNotch,         // обычная риска
                      camAngleRulerLongNotch,        // длинная риска
                      camAngleRullerLineWidth,         // толщина линии
                      camAngleRulerColor,
                      camAngleRulerLeft,
                      camAngleRulerDrawLabels,
                      camAngleRulerLabelsOffset,
                      labelFont,
                      [](int i) {   // форматтер
                          // return QString("%1 m").arg(i * 10.0, 0, 'i', 1);  // Например: 0.0 cm, 2.5 cm ...
                          return QString::number(-(i * 18 - 90));
                      },
                      camAngleRulerDrawPoimter,
                      camAngleRulerPointerPos,
                      camAngleRulerPointerValue,
                      camAngleRulerPointerSize,
                      camAngleRulerPointerHollow,
                      camAngleRulerPointerOffset,
                      camAngleRuleTitle,
                      camAngleRulerTitleOffset);

    //Вертикальная линейка дифферента
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
    bool pitchRulerDrawPoimter = true;
    int pitchRulerPointerSize = 8;
    bool pitchRulerPointerHollow = true;
    int pitchRulerPointerOffset = -5;
    double pitchRulerPointerPos = (double(90.0f - oPitch) / 180.0f);
    QString pitchRulerPointerValue = QString::number(std::round(oPitch));
    QString pitchRuleTitle = "Дифферент";
    int pitchRulerTitleOffset = -20;
    bool pitchRulerDrawSetpoint = ostabPitch;
    double pitchRulerSetpointPos = (double(90.0f - oPitchSetpoint) / 180.0f);
    QString pitchRulerSetpointValue = QString::number(std::round(oPitchSetpoint));
    int pitchRulerSetpointSize = pitchRulerPointerSize/2;
    int pitchRulerSetpointHollow = false;
    int pitchRulerSetpointOffset = pitchRulerPointerOffset + pitchRulerSetpointSize/2;
    int pitchRulerSetpointHideTreshold = 12;

    drawVerticalRuler(&painter,
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
                      [](int i) {
                          return QString::number(-(i * 6 - 90));
                      },
                      pitchRulerDrawPoimter,
                      pitchRulerPointerPos,
                      pitchRulerPointerValue,
                      pitchRulerPointerSize,
                      pitchRulerPointerHollow,
                      pitchRulerPointerOffset,
                      pitchRuleTitle,
                      pitchRulerTitleOffset,
                      pitchRulerDrawSetpoint,
                      pitchRulerSetpointPos,
                      pitchRulerSetpointValue,
                      pitchRulerSetpointSize,
                      pitchRulerSetpointHollow,
                      pitchRulerSetpointOffset,
                      pitchRulerSetpointHideTreshold);


    //Горизонтальная линейка крена
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
    bool rollRulerDrawPoimter = true;
    int rollRulerPointerSize = 8;
    bool rollRulerPointerHollow = true;
    int rollRulerPointerOffset = -5;
    double rollRulerPointerPos = (double(90.0f + oRoll) / 180.0f);
    QString rollRulerPointerValue = QString::number(std::round(oRoll));
    QString rollRuleTitle = "Крен";
    int rollRulerTitleOffset = 20;
    bool rollRulerDrawSetpoint = ostabRoll;
    double rollRulerSetpointPos = (double(90.0f + oRollSetpoint) / 180.0f);
    QString rollRulerSetpointValue = QString::number(std::round(oRollSetpoint));
    int rollRulerSetpointSize = rollRulerPointerSize/2;
    int rollRulerSetpointHollow = false;
    int rollRulerSetpointOffset = rollRulerPointerOffset + rollRulerSetpointSize/2;
    int rollRulerSetpointHideTreshold = 12;

    drawHorizontalRuler(&painter,
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
                      [](int i) {
                          return QString::number((i * 6 - 90));
                      },
                      rollRulerDrawPoimter,
                      rollRulerPointerPos,
                      rollRulerPointerValue,
                      rollRulerPointerSize,
                      rollRulerPointerHollow,
                      rollRulerPointerOffset,
                      rollRuleTitle,
                      rollRulerTitleOffset,
                      rollRulerDrawSetpoint,
                      rollRulerSetpointPos,
                      rollRulerSetpointValue,
                      rollRulerSetpointSize,
                      rollRulerSetpointHollow,
                      rollRulerSetpointOffset,
                      rollRulerSetpointHideTreshold);

    //Скользящая линейка глубины
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
    int depthRulerTitleOffset = pitchRulerTitleOffset;
    bool depthRulerShowPointerLabel = true;
    QString depthRulerValueName = QString::number(depthRulerValue, 'f', 1);
    int depthRulerPointerSize = pitchRulerPointerSize;
    int depthRulerPointerOffset = -pitchRulerPointerOffset;
    int depthRulerPointerLabelOffset = depthRulerPointerOffset + 14;

    drawVerticalSlidingRuler(&painter,
                             depthRulerPosition,
                             depthRulerNumNotches,
                             depthRulerNotchSpacing,               // 21 деление, шаг 12 px
                             depthRulerNotchShort,
                             depthRulerNotchLong,                // короткая/длинная риска
                             depthRulerLineWidth,
                             depthRulerColor,
                             depthRulerValue,                  // текущее значение
                             depthRulerStep,
                             labelFont,
                             [](int v) { return QString::number(v); },
                             depthRulerPointerHolow,                // hollowPointer
                             depthRulerTitle,
                             depthRulerTitleOffset,
                             depthRulerShowPointerLabel,
                             depthRulerValueName,
                             depthRulerPointerLabelOffset,
                             depthRulerPointerSize,
                             depthRulerPointerOffset);

    //Скользящая горизонтальная линейка для курса
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
    int yawRulerTitleOffset = pitchRulerTitleOffset;
    bool yawRulerShowPointerLabel = true;
    QString yawRulerValueName = QString::number(std::round(yawRulerValue));
    int yawRulerPointerSize = pitchRulerPointerSize;
    int yawRulerPointerOffset = -pitchRulerPointerOffset;
    int yawRulerPointerLabelOffset = yawRulerPointerOffset;

    drawHorizontalSlidingRuler(&painter,
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
                             yawRulerPointerHolow,                             yawRulerPointerSize,
                             yawRulerPointerOffset,                // hollowPointer
                             yawRulerTitle,
                             yawRulerTitleOffset,
                             yawRulerShowPointerLabel,
                             yawRulerValueName,
                             yawRulerPointerLabelOffset);

    //Заряд батареи
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
    drawBatteryIcon(&painter,
                    batteryArea,
                    batteryValue,
                    defaultColor,
                    batteryColor,
                    batteryBorderWidth,
                    true,
                    true,
                    batteryFont,
                    batteryTextColor);

    //Счетчик оборотов аппарата
    QRect revolutionCounterRect(screenWidth / 30, screenHeight/12 + 20, 120, 20);
    QFont revFont("Consolas", 12, QFont::Bold);
    QColor revColor = defaultColor;
    painter.setFont(revFont);
    painter.setPen(revColor);
    painter.drawText(revolutionCounterRect, Qt::AlignLeft, "Обороты: " + QString::number(revolutionCount));

    //Состояние светильников
    QRect lightsRect(screenWidth / 30, screenHeight/12 + 40, 220, 20);
    QFont lightsFont("Consolas", 12, QFont::Bold);
    QColor lightsColor = defaultColor;
    painter.setFont(lightsFont);
    painter.setPen(lightsColor);
    if(oLightsState)
        painter.drawText(lightsRect, Qt::AlignLeft, "Освещение: вкл.");
    else
        painter.drawText(lightsRect, Qt::AlignLeft, "Освещение: выкл.");
    // // Рисуем оверлей на всей доступной области виджета
    // painter.setBrush(QBrush(QColor(255, 0, 0, 100))); // Будет красить
    // painter.drawRect(rect()); // Используем rect() для получения текущих размеров виджета

    //Пример оверлея: красная линия от угла к углу
    // painter.setPen(QPen(Qt::red, 2));
    // painter.drawLine(width()/2, 0, width()/2, height());
    // painter.drawLine(0, height()/2, width(), height()/2);
    // // Пример оверлея: текст в центре
    // painter.setPen(Qt::white);
    // painter.setFont(QFont("Arial", 12));
    // painter.drawText(rect(), Qt::AlignCenter, "Overlay Example");

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
    ocamAngle = constrainff(camAngle, -90.0f, 90.0f);
    oLightsState = lightsState;
}

void OverlayWidget::updateOverlay(){
    emit requestOverlayDataUpdate();
    this->setGeometry(0, 0, parentWidget->width(), parentWidget->height());
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
