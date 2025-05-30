#ifndef CAMERA_H
#define CAMERA_H

#include <QObject>
#include <QDebug>
#include <QList>
#include <QStringList>
#include <QTimer>
#include <set>
#include <filesystem>
#include <sstream>
#include <ctime>
#include <windows.h>
#include <opencv2/opencv.hpp>
#include "MvCameraControl.h"
#include "camera_structs.h"
#include "camera_worker.h"
#include "frame_processor.h"
#include "video_recorder.h"
#include "video_streamer.h"

class Camera : public QObject {
    Q_OBJECT
public:
    explicit Camera(QObject* parent = nullptr);
    ~Camera();

    void start();
    void stopAll();
    void startRecording(const QString& cameraName, int recordInterval = 30, int storedVideoFilesLimit = 100);
    void stopRecording(const QString& cameraName);
    void startStreaming(const QString& cameraName, int port);
    void stopStreaming(const QString& cameraName);
    void stereoShot();
    const QList<CameraFrameInfo*>& getCameras() const;
    void setCameraNames(const QStringList& names);
    QStringList getCameraNames() const;
    void initializeCameras();

private:
    int checkCameras();
    int destroyCameras(void* handle);
    void getHandle(unsigned int cameraID, void** handle, const std::string& cameraName);
    void cleanupAllCameras();
    void reconnectCameras();
    void reinitializeCameras();

private slots:
    void handleCaptureFailure(const QString& reason);
    void handleRecordingFailure(const QString& reason);
    void handleStreamingFailure(const QString& reason);

signals:
    void frameReady(CameraFrameInfo* camera);
    void recordingStarted(CameraFrameInfo* camera);
    void recordingFinished(CameraFrameInfo* camera);
    void streamingStarted(CameraFrameInfo* camera);
    void streamingFinished(CameraFrameInfo* camera);
    void stereoShotSaved(const QString& filePaths);
    void stereoShotFailed(const QString& reason);
    void finished();
    void reconnectDone(Camera* camera);
    void errorOccurred(const QString& component, const QString& message);
    void greatSuccess(const QString& component, const QString& message);

private:
    MV_CC_DEVICE_INFO_LIST m_deviceList;
    QList<CameraFrameInfo*> m_cameras;
    QList<CameraVideoFrameInfo*> m_videoInfos;
    QStringList m_cameraNames;
    std::set<unsigned int> m_usedIPs;
    int m_reconnectAttempts;
    const int m_maxReconnectAttempts;
    QTimer* m_checkCameraTimer;
};

#endif // CAMERA_H
