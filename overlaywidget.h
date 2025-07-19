#ifndef OVERLAYWIDGET_H
#define OVERLAYWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include "udptelemetryparser.h"
#include <QColor>
#include <QPoint>

class OverlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OverlayWidget(QWidget *parent = nullptr);
    void telemetryUpdate(TelemetryPacket& telemetry);
    void controlsUpdate(const bool& stabEnabled,
                        const bool& stabRoll,
                        const bool& stabPitch,
                        const bool& stabYaw,
                        const bool& stabDepth,
                        const bool& masterFlag,
                        const float& powerLimit,
                        const float& camAngle);

public slots:

signals:
    void requestOverlayDataUpdate();

private:
    QTimer *frameTimer;

    void updateOverlay();

    bool ostabEnabled;
    bool ostabRoll;
    bool ostabPitch;
    bool ostabYaw;
    bool ostabDepth;
    bool omasterFlag;
    float opowerLimit;
    float ocamAngle;
    QWidget *parentWidget;

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // OVERLAYWIDGET_H
