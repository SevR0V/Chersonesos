#ifndef OVERLAYWIDGET_H
#define OVERLAYWIDGET_H

#include <QWidget>
#include <QPainter>
#include "udptelemetryparser.h"

class OverlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OverlayWidget(QWidget *parent = nullptr);

public slots:
    void telemetryUpdate(TelemetryPacket& telemetry);
    void controlsUpdate(bool& stabRoll,
                        bool& stabPitch,
                        bool& stabYaw,
                        bool& stabDepth,
                        bool& masterFlag,
                        float& powerLimit);
private:
    float CENTER_ALIGMENT_X = 0.5f;
    float CENTER_ALIGMENT_Y = 0.5f;
    float DEPTH_ALIGMENT_X = 0.75f;
    float DEPTH_ALIGMENT_Y = 0.5f;
    float COMPASS_ALIGMENT_X = 0.5f;
    float COMPASS_ALIGMENT_y = 0.15f;

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // OVERLAYWIDGET_H
