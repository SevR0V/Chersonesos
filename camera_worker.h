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
    explicit CameraWorker(CameraFrameInfo* frameInfo, StreamFrameInfo* streamInfo, RecordFrameInfo* recordInfo, OverlayFrameInfo* overlayInfo, StereoFrameInfo* stereoInfo, QObject* parent = nullptr);
    ~CameraWorker();

public slots:
    void capture();
    void stop();

private:
    void cleanupCamera();
    CameraFrameInfo* m_frameInfo;
    StreamFrameInfo* m_streamInfo;
    RecordFrameInfo* m_recordInfo;
    OverlayFrameInfo* m_overlayInfo;
    StereoFrameInfo* m_stereoInfo;
    bool m_isRunning;
    bool isLeftCamera = false;

signals:
    void frameReady();
    //void frameDataReady(const QByteArray& frameData, int width, int height, unsigned int pixelType);
    void errorOccurred(const QString& component, const QString& message);
    void captureFailed(const QString& reason);
};

#endif // CAMERA_WORKER_H
