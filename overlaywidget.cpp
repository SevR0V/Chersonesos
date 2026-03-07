#include "overlaywidget.h"
#include <QScreen>
#include <QWindow>
#include <QFontDatabase>
#include <cmath>

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

    ofThrust = 0;
    osThrust = 0;
    orThrust = 0;
    ovThrust = 0;

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
        int txtXOffset = 8;
        int textX = pointerLeft
                        ? baseX - textRect.width() - txtXOffset
                        : baseX + txtXOffset;

        // int textY = pointerY + textRect.height() / 2 - fm.descent();

        QRect labelRect(textX, pointerY - textRect.height() / 2,
                        textRect.width()+10, textRect.height());
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

void drawCircularCompass(QPainter* painter,
                         const QPoint& center,
                         int radius,
                         double headingDeg,
                         const QColor& color,
                         const QFont& baseFont = QFont(),
                         int lineWidth = 2,
                         bool showHeadingText = true,
                         const QString& title = QString())
{
    if (!painter || radius < 20)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int cx = center.x();
    const int cy = center.y();

    double heading = std::fmod(headingDeg, 360.0);
    if (heading < 0.0)
        heading += 360.0;

    // Все размеры через радиус, чтобы компас нормально ужимался
    const int outerRingWidth   = std::max(1, radius / 22);
    const int innerRadius      = std::max(8, int(radius * 0.72));

    const int majorCardinalLen = std::max(8, int(radius * 0.18)); // N/E/S/W
    const int majorLen         = std::max(6, int(radius * 0.11)); // 30°
    const int minorLen         = std::max(3, int(radius * 0.06)); // 10°

    const int northArrowLen    = std::max(10, int(radius * 0.16));
    const int northArrowWidth  = std::max(6, int(radius * 0.10));

    const int headingArrowLen  = std::max(10, int(innerRadius * 0.42));
    const int headingArrowHalfW= std::max(4, int(innerRadius * 0.10));

    const int centerDotR       = std::max(2, radius / 24);
    const int cardinalOffset   = std::max(10, int(radius * 0.20));
    const int headingTextGap   = std::max(10, int(radius * 0.16));
    const int titleGap         = std::max(8, int(radius * 0.12));

    QPen pen(color);
    pen.setWidth(outerRingWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    // Кольца
    painter->drawEllipse(center, radius, radius);
    painter->drawEllipse(center, innerRadius, innerRadius);

    // Риски
    for (int markDeg = 0; markDeg < 360; markDeg += 10) {
        const bool isCardinal = (markDeg % 90 == 0);
        const bool isMajor    = (markDeg % 30 == 0);

        int tickLen = minorLen;
        if (isCardinal)
            tickLen = majorCardinalLen;
        else if (isMajor)
            tickLen = majorLen;

        double displayDeg = double(markDeg) - heading;
        double rad = displayDeg * M_PI / 180.0;

        QPointF p1(cx + std::sin(rad) * (radius - tickLen),
                   cy - std::cos(rad) * (radius - tickLen));
        QPointF p2(cx + std::sin(rad) * radius,
                   cy - std::cos(rad) * radius);

        painter->drawLine(p1, p2);
    }

    // Подписи сторон света — поворачиваются вместе с компасом
    struct CardinalLabel {
        int deg;
        QString text;
    };

    const CardinalLabel labels[] = {
        {  0, "N" },
        { 90, "E" },
        {180, "S" },
        {270, "W" }
    };

    QFont cardinalFont = baseFont;
    cardinalFont.setBold(true);
    cardinalFont.setPixelSize(std::max(9, int(radius * 0.18)));
    painter->setFont(cardinalFont);

    QFontMetrics cfm(cardinalFont);

    for (const auto& item : labels) {
        double displayDeg = double(item.deg) - heading;
        double rad = displayDeg * M_PI / 180.0;

        QPointF pos(cx + std::sin(rad) * (radius - majorCardinalLen - cardinalOffset),
                    cy - std::cos(rad) * (radius - majorCardinalLen - cardinalOffset));

        painter->save();
        painter->translate(pos);

        // Поворачиваем букву по кругу вместе с компасом
        painter->rotate(displayDeg);

        QRect r = cfm.boundingRect(item.text);
        painter->drawText(QPoint(-r.width() / 2, r.height() / 2 - cfm.descent()), item.text);
        painter->restore();
    }

    // Стрелка СЕВЕРА — вращается вместе с севером
    {
        double northDisplayDeg = -heading;
        double rad = northDisplayDeg * M_PI / 180.0;

        QPointF dir(std::sin(rad), -std::cos(rad));
        QPointF normal(-dir.y(), dir.x());

        QPointF tip  = QPointF(cx, cy) + dir * (radius - 2);
        QPointF base = QPointF(cx, cy) + dir * (radius - northArrowLen - 2);

        QPolygonF northArrow;
        northArrow << tip
                   << (base + normal * northArrowWidth * 0.5)
                   << (base - normal * northArrowWidth * 0.5);

        painter->save();
        painter->setBrush(color);
        painter->drawPolygon(northArrow);
        painter->restore();
    }

    // Стрелка направления аппарата — всегда вверх, но внутри внутреннего радиуса
    {
        QPoint tip(cx, cy - headingArrowLen);
        QPoint left(cx - headingArrowHalfW, cy + headingArrowHalfW);
        QPoint right(cx + headingArrowHalfW, cy + headingArrowHalfW);

        QPolygon headingArrow;
        headingArrow << tip << left << right;

        painter->save();
        painter->setBrush(color);
        painter->drawPolygon(headingArrow);
        painter->restore();
    }

    // Центральная точка
    painter->setBrush(color);
    painter->drawEllipse(center, centerDotR, centerDotR);
    painter->setBrush(Qt::NoBrush);

    // Текущий угол — снизу под компасом
    if (showHeadingText) {
        QFont valueFont = baseFont;
        valueFont.setBold(true);
        valueFont.setPixelSize(std::max(9, int(radius * 0.16)));
        painter->setFont(valueFont);

        QString headingText = QString::number(int(std::round(heading))) + QChar(0x00B0);
        QFontMetrics vfm(valueFont);
        QRect tr = vfm.boundingRect(headingText);

        QPoint textPos(cx - tr.width() / 2,
                       cy + radius + headingTextGap + tr.height());

        painter->drawText(textPos, headingText);
    }

    // Заголовок ещё ниже, если нужен
    if (!title.isEmpty()) {
        QFont titleFont = baseFont;
        titleFont.setPixelSize(std::max(8, int(radius * 0.13)));
        painter->setFont(titleFont);

        QFontMetrics tfm(titleFont);
        QRect tr = tfm.boundingRect(title);

        int y = cy + radius + headingTextGap
                + std::max(12, int(radius * 0.18))
                + titleGap + tr.height();

        painter->drawText(QPoint(cx - tr.width() / 2, y), title);
    }

    painter->restore();
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

#include <cmath>

#include <cmath>

#include <cmath>

void drawCompassThrustIndicator(QPainter* painter,
                                const QPoint& center,
                                int compassRadius,
                                double ofThrust,   // forward/back   [-1..1]
                                double osThrust,   // strafe         [-1..1]
                                double orThrust,   // yaw rotation   [-1..1]
                                double ovThrust,   // vertical       [-1..1]
                                const QColor& color,

                                // offsets / sizes
                                int arcBaseOffset = 10,          // отступ дуг движения от компаса
                                int arcLevelSpacing = 8,         // расстояние между уровнями дуг
                                int arcThickness = 3,            // толщина линий
                                int arcSpanDeg = 52,             // ширина дуг движения

                                int rotArcOffset = 22,           // отступ дуговых стрелок вращения
                                int rotArcSpacing = 8,           // расстояние между уровнями вращения
                                int rotArcSpanDeg = 42,          // длина дуговой стрелки
                                int rotArrowHeadSize = 8,        // размер наконечника дуговой стрелки

                                int vertOffset = 22,             // отступ вертикального индикатора
                                int vertSegLen = 8,              // длина сегмента
                                int vertSegGap = 4,              // зазор между сегментами
                                int vertArrowHeadSize = 7,       // размер наконечника вертикальной стрелки
                                bool verticalOnRight = true)     // справа или слева
{
    if (!painter || compassRadius < 10)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    auto clamp01 = [](double v) -> double {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    };

    const int cx = center.x();
    const int cy = center.y();

    QPen pen(color);
    pen.setWidth(std::max(1, arcThickness));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    auto pointOnCircleCustom = [&](double customDeg, double radius) -> QPointF {
        double rad = customDeg * M_PI / 180.0;
        return QPointF(cx + std::sin(rad) * radius,
                       cy - std::cos(rad) * radius);
    };

    // ============================================================
    // 1. Горизонтальная тяга: дуги "wifi"
    // ============================================================
    double hx = osThrust;
    double hy = -ofThrust;

    double hMag = std::sqrt(hx * hx + hy * hy);
    double hNorm = clamp01(hMag);

    int hLevels = 0;
    if (hNorm > 0.15) hLevels = 1;
    if (hNorm > 0.45) hLevels = 2;
    if (hNorm > 0.75) hLevels = 3;

    double moveAngleDeg = 0.0;
    if (hNorm > 0.001) {
        moveAngleDeg = std::atan2(hx, -hy) * 180.0 / M_PI; // 0 = вверх
        if (moveAngleDeg < 0.0)
            moveAngleDeg += 360.0;
    }

    for (int i = 0; i < hLevels; ++i) {
        int r = compassRadius + arcBaseOffset + i * arcLevelSpacing;

        double qtCenterDeg = 90.0 - moveAngleDeg;
        int startAngle16 = int(std::round((qtCenterDeg - arcSpanDeg / 2.0) * 16.0));
        int spanAngle16  = int(std::round(arcSpanDeg * 16.0));

        QRect rect(cx - r, cy - r, 2 * r, 2 * r);
        painter->drawArc(rect, startAngle16, spanAngle16);
    }

    // ============================================================
    // 2. Вращение по курсу: дуговые стрелки по касательной
    // ============================================================
    double rNorm = clamp01(std::abs(orThrust));
    int rLevels = 0;
    if (rNorm > 0.15) rLevels = 1;
    if (rNorm > 0.45) rLevels = 2;
    if (rNorm > 0.75) rLevels = 3;

    int baseOuterR = compassRadius + arcBaseOffset + std::max(0, hLevels - 1) * arcLevelSpacing;
    int rotBaseR = baseOuterR + rotArcOffset;

    auto drawTangentialArrowHead = [&](double tipDeg, double radius, bool clockwise)
    {
        QPointF tip = pointOnCircleCustom(tipDeg, radius);

        double tangentDeg = clockwise ? (tipDeg + 90.0) : (tipDeg - 90.0);
        double tangentRad = tangentDeg * M_PI / 180.0;

        QPointF dir(std::sin(tangentRad), -std::cos(tangentRad));
        QPointF back = -dir;
        QPointF normal(-dir.y(), dir.x());

        double headLen = std::max(5, rotArrowHeadSize);
        double headWing = std::max(3, int(rotArrowHeadSize * 0.55));

        QPointF p1 = tip + back * headLen + normal * headWing;
        QPointF p2 = tip + back * headLen - normal * headWing;

        painter->drawLine(tip, p1);
        painter->drawLine(tip, p2);
    };

    auto drawRotArcArrow = [&](double centerDeg, double radius, double spanDeg, bool clockwise)
    {
        double startCustomDeg = clockwise ? (centerDeg - spanDeg / 2.0)
                                          : (centerDeg + spanDeg / 2.0);

        double endCustomDeg   = clockwise ? (centerDeg + spanDeg / 2.0)
                                          : (centerDeg - spanDeg / 2.0);

        double startQtDeg = 90.0 - startCustomDeg;
        double spanQtDeg  = clockwise ? -spanDeg : spanDeg;

        QRect rect(cx - int(radius), cy - int(radius), int(radius * 2), int(radius * 2));
        painter->drawArc(rect,
                         int(std::round(startQtDeg * 16.0)),
                         int(std::round(spanQtDeg * 16.0)));

        drawTangentialArrowHead(endCustomDeg, radius, clockwise);
    };

    // ВАЖНО:
    // при отсутствии тяги вращения не рисуем ничего
    if (rLevels > 0) {
        for (int i = 0; i < rLevels; ++i) {
            int r = rotBaseR + i * rotArcSpacing;

            if (orThrust > 0.0) {
                // Положительное вращение: правая сторона, дуга по часовой
                drawRotArcArrow(90.0, r, rotArcSpanDeg, true);
            } else {
                // Отрицательное вращение: левая сторона, дуга против часовой
                drawRotArcArrow(270.0, r, rotArcSpanDeg, false);
            }
        }
    }

    // ============================================================
    // 3. Вертикальная тяга: сегменты + наконечник
    // ============================================================
    double vNorm = clamp01(std::abs(ovThrust));
    int vLevels = 0;
    if (vNorm > 0.15) vLevels = 1;
    if (vNorm > 0.45) vLevels = 2;
    if (vNorm > 0.75) vLevels = 3;

    if (vLevels > 0) {
        int side = verticalOnRight ? 1 : -1;
        int vx = cx + side * (compassRadius + vertOffset);

        int dir = (ovThrust >= 0.0) ? -1 : 1; // вверх = -1, вниз = +1
        int yStart = cy + dir * 6;

        for (int i = 0; i < vLevels; ++i) {
            int segOffset = i * (vertSegLen + vertSegGap);

            int y1 = yStart + dir * segOffset;
            int y2 = y1 + dir * vertSegLen;

            painter->drawLine(QPoint(vx, y1), QPoint(vx, y2));
        }

        QPoint tip(vx, yStart + dir * (vLevels * (vertSegLen + vertSegGap)));
        int wing = std::max(4, vertArrowHeadSize);

        if (dir < 0) {
            painter->drawLine(tip, QPoint(vx - wing, tip.y() + vertArrowHeadSize));
            painter->drawLine(tip, QPoint(vx + wing, tip.y() + vertArrowHeadSize));
        } else {
            painter->drawLine(tip, QPoint(vx - wing, tip.y() - vertArrowHeadSize));
            painter->drawLine(tip, QPoint(vx + wing, tip.y() - vertArrowHeadSize));
        }
    }

    painter->restore();
}

template<typename T>
T constrain(T x, T min_val, T max_val) {
    if (x < min_val) return min_val;
    if (x > max_val) return max_val;
    return x;
}

template<typename T>
T map(T x, T in_min, T in_max, T out_min, T out_max) {
    return (x - in_min) * (out_max - out_min) /
               (in_max - in_min) + out_min;
}

float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) /
               (in_max - in_min) + out_min;
}

void OverlayWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);


    int screenWidth = width();

    int screenHeight = height();
    int CameraVerticalAngle = 56;


    int fontSize = constrain(map(screenWidth, 800, 2400, 10, 18),10,18);
    QFont labelFont("Cascadia Code", fontSize);
    QFont bigLabelFont("Cascadia Code", fontSize + 4);

    QColor defaultColor(Qt::red);

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
    // int camAngleRulerTitleOffset = -20;
    int camAngleRulerTitleOffset = map(fontSize, 10, 18, -20, -100);

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
    int yawRulerWidth = screenWidth/8*5;
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
    QRect revolutionCounterRect(screenWidth / 30, screenHeight/12 + 20, 140, 25);
    QFont revFont("Consolas", 12, QFont::Bold);
    QColor revColor = defaultColor;
    painter.setFont(labelFont);
    painter.setPen(revColor);
    painter.drawText(revolutionCounterRect, Qt::AlignLeft, "Обороты: " + QString::number(revolutionCount));
    //Состояние светильников
    QRect lightsRect(screenWidth / 30, screenHeight/12 + 50, 220, 25);
    QFont lightsFont("Consolas", 12, QFont::Bold);
    QColor lightsColor = defaultColor;
    painter.setFont(labelFont);
    painter.setPen(lightsColor);
    if(oLightsState)
        painter.drawText(lightsRect, Qt::AlignLeft, "Освещение: вкл.");
    else
        painter.drawText(lightsRect, Qt::AlignLeft, "Освещение: выкл.");

    // Круглый компас
    int compassOffset = constrain(map(screenWidth, 800, 2400, 30, 70), 30, 70);
    int compassRadius = constrain(map(screenWidth, 800, 2400, 35, 85), 35, 85);

    QPoint compassCenter(screenWidth - compassRadius - compassOffset,
                         compassRadius + compassOffset);

    double compassHeading = std::fmod(oYaw + 360.0, 360.0);

    drawCircularCompass(&painter,
                        compassCenter,
                        compassRadius,
                        compassHeading,
                        defaultColor,
                        labelFont,
                        2,
                        true,
                        "");



    // ofThrust = 0.3;
    // osThrust = -0.2;
    // ovThrust = 0.5;
    // orThrust = 0.5;

    drawCompassThrustIndicator(&painter,
                               compassCenter,
                               compassRadius,
                               ofThrust,
                               osThrust,
                               orThrust,
                               ovThrust,
                               defaultColor,
                               3,   // arcBaseOffset
                               8,    // arcLevelSpacing
                               1,    // arcThickness
                               15,   // arcSpanDeg
                               22,   // rotArcOffset
                               8,    // rotArcSpacing
                               42,   // rotArcSpanDeg
                               8,    // rotArrowHeadSize
                               22,   // vertOffset
                               8,    // vertSegLen
                               4,    // vertSegGap
                               7,    // vertArrowHeadSize
                               true  // verticalOnRight
                               );
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
                                   const bool& lightsState,
                                   const float& fThrust,
                                   const float& sThrust,
                                   const float& rThrust,
                                   const float& vThrust){
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
