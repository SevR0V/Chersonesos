#ifndef FRAME_PROCESSOR_H
#define FRAME_PROCESSOR_H

#include <QObject>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include "camera_structs.h"

class FrameProcessor : public QObject {
    Q_OBJECT
public:
    explicit FrameProcessor(CameraVideoFrameInfo* videoInfo, QObject* parent = nullptr);

public slots:
    void processFrame(const QByteArray& frameData, int width, int height, int pixelType);

signals:
    void errorOccurred(const QString& component, const QString& message);

private:
    CameraVideoFrameInfo* m_videoInfo;
};

#endif // FRAME_PROCESSOR_H
