#include "overlaywidget.h"

OverlayWidget::OverlayWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents); // Прозрачный для событий мыши
    setStyleSheet("background-color: transparent;"); // Полностью прозрачный фон

    frameTimer = new QTimer(this);
    connect(frameTimer, &QTimer::timeout, this, &OverlayWidget::updateOverlay);
    frameTimer->start(1000/60);
    ostabEnabled = 0;
    ostabRoll = 0;
    ostabPitch = 0;
    ostabYaw = 0;
    ostabDepth = 0;
    omasterFlag = 0;
    opowerLimit = 0;
    ocamAngle = 0;
    parentWidget = parent;
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
                       int titleOffset = 4) {
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
        QRect textRect = fm.boundingRect(pointerLabel);

        int textX = pointerLeft
                        ? baseX - textRect.width() - 4
                        : baseX + 4;

        int textY = pointerY + textRect.height() / 2 - fm.descent();

        painter->drawText(QPoint(textX, textY), pointerLabel);
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

void OverlayWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont labelFont("Consolas", 8);
    int screenWidth = width();

    int screenHeight = height();
    int CameraVerticalAngle = 56;

    QColor defaultColor(Qt::gray);

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
    int camY = centerY + ocamAngle * pixInDeg;
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
    int camAngleRulerNotchSpacing = camAngleRulerHeight / camAngleRulerNumNotches;
    QPoint camAngleRulerPos(80, screenHeight - camAngleRulerHeight - camAngleRulerHeight / 3);
    int camAngleRulerShortNotch = 5;
    int camAngleRulerLongNotch = 12;
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
    QString camAngleRuleTitle = "Угол камеры:";
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
    // // Рисуем оверлей на всей доступной области виджета
    // painter.setBrush(QBrush(QColor(255, 0, 0, 100))); // Будет красить
    // painter.drawRect(rect()); // Используем rect() для получения текущих размеров виджета

    // // Пример оверлея: красная линия от угла к углу
    // painter.setPen(QPen(Qt::red, 2));
    // painter.drawLine(10, 10, width() - 10, height() - 10);

    // // Пример оверлея: текст в центре
    // painter.setPen(Qt::white);
    // painter.setFont(QFont("Arial", 12));
    // painter.drawText(rect(), Qt::AlignCenter, "Overlay Example");

}

void OverlayWidget::telemetryUpdate(TelemetryPacket& telemetry){

}

void OverlayWidget::controlsUpdate(const bool& stabEnabled,
                                   const bool& stabRoll,
                                   const bool& stabPitch,
                                   const bool& stabYaw,
                                   const bool& stabDepth,
                                   const bool& masterFlag,
                                   const float& powerLimit,
                                   const float& camAngle){
    ostabEnabled = stabEnabled;
    ostabRoll = stabRoll;
    ostabPitch = stabPitch;
    ostabYaw = stabYaw;
    ostabDepth = stabDepth;
    omasterFlag = masterFlag;
    opowerLimit = powerLimit;
    ocamAngle = camAngle < -90 ? -90 : camAngle;
    ocamAngle = camAngle > 90 ? 90 : camAngle;

}

void OverlayWidget::updateOverlay(){
    emit requestOverlayDataUpdate();
    this->setGeometry(0, 0, parentWidget->width(), parentWidget->height());
    this->update();
}
