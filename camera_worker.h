#ifndef CAMERA_WORKER_H
#define CAMERA_WORKER_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <QMutex>
#include "camera_structs.h"
#include "MvCameraControl.h"

class CameraWorker : public QObject {
    Q_OBJECT
public:
    explicit CameraWorker(CameraFrameInfo* frameInfo, CameraVideoFrameInfo* videoInfo, QObject* parent = nullptr);
    ~CameraWorker();

    void stop();

public slots:
    void capture();

signals:
    void frameReady();
    void frameDataReady(const QByteArray& frameData, int width, int height, int pixelType);
    void captureFailed(const QString& reason);
    void errorOccurred(const QString& component, const QString& message);

private:
    void cleanupCamera();

    CameraFrameInfo* m_frameInfo;
    CameraVideoFrameInfo* m_videoInfo;
    bool m_isRunning;
};

#endif // CAMERA_WORKER_H
