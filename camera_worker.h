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
    explicit CameraWorker(CameraFrameInfo* frameInfo, StreamFrameInfo* streamInfo, RecordFrameInfo* recordInfo, QObject* parent = nullptr);
    ~CameraWorker();

public slots:
    void capture();
    void stop();

private:
    void cleanupCamera();

private:
    CameraFrameInfo* m_frameInfo;
    StreamFrameInfo* m_streamInfo;
    RecordFrameInfo* m_recordInfo;
    bool m_isRunning;

signals:
    void frameReady();
    //void frameDataReady(const QByteArray& frameData, int width, int height, unsigned int pixelType);
    void errorOccurred(const QString& component, const QString& message);
    void captureFailed(const QString& reason);
};

#endif // CAMERA_WORKER_H
