#ifndef OVERLAYWIDGET_H
#define OVERLAYWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include "udptelemetryparser.h"
#include <QColor>
#include <QPoint>
#include <opencv2/opencv.hpp>

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
                        const float& camAngle,
                        const bool &lightsState);

    cv::Mat getOverlayAsMat(int width, int height);

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
    bool oLightsState;
    float opowerLimit;
    float ocamAngle;
    float oPitch;
    float oRoll;
    float oYaw;
    float oDepth;
    float oPitchSetpoint;
    float oRollSetpoint;
    float oYawSetpoint;
    float oDepthSetpoint;
    float oBatLevel;
    float prevYaw;
    float revolutionCount;
    QWidget *parentWidget;

    void countRevolutions();
    void drawOverlay(QPainter* painter, int width, int height);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // OVERLAYWIDGET_H
