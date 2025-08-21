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
#include "video_recorder.h"
#include "video_streamer.h"

class CameraWorker;
class VideoRecorder;
class VideoStreamer;

class Camera : public QObject {
    Q_OBJECT
public:
    explicit Camera(QStringList& names, QObject* parent = nullptr);
    ~Camera();

    const QList<CameraFrameInfo*>& getCameras() const;
    QStringList getCameraNames() const;
    void setCameraNames(const QStringList& names);

public slots:
    void startCamera();
    void stopAllCameras();
    void startRecordingSlot(const QString& cameraName, int recordInterval, int storedVideoFilesLimit);
    void stopRecordingSlot(const QString& cameraName);
    void startStreamingSlot(const QString& cameraName, int port);
    void stopStreamingSlot(const QString& cameraName);
    void stereoShotSlot();

signals:
    void frameReady(CameraFrameInfo* camera);
    void errorOccurred(const QString& component, const QString& message);
    void greatSuccess(const QString& component, const QString& message);
    void reconnectDone(Camera* camera);
    void recordingStarted(CameraFrameInfo* camera);
    void recordingFinished(CameraFrameInfo* camera);
    void recordingFailed(const QString& reason);
    void streamingStarted(CameraFrameInfo* camera);
    void streamingFinished(CameraFrameInfo* camera);
    void streamingFailed(const QString& reason);
    void stereoShotSaved(const QString& filePaths);
    void stereoShotFailed(const QString& reason);
    void finished();

private:
    QStringList m_cameraNames;
    QList<CameraFrameInfo*> m_cameras;
    QList<StreamFrameInfo*> m_streamInfos;
    QList<RecordFrameInfo*> m_recordInfos;
    MV_CC_DEVICE_INFO_LIST m_deviceList;
    QTimer* m_checkCameraTimer;
    int m_reconnectAttempts;
    std::set<unsigned int> m_usedIPs;
    std::filesystem::path m_sessionDirectory; // Путь к сессионной папке

    int checkCameras();
    void initializeCameras();
    void reinitializeCameras();
    void start();
    void stopAll();
    void startRecording(const QString& cameraName, int recordInterval, int storedVideoFilesLimit);
    void stopRecording(const QString& cameraName);
    void startStreaming(const QString& cameraName, int port);
    void stopStreaming(const QString& cameraName);
    void stereoShot();
    int destroyCameras(void* handle);
    void getHandle(unsigned int cameraID, void** handle, const std::string& cameraName);
    void cleanupAllCameras();
    void reconnectCameras();
    void handleCaptureFailure(const QString& reason);
    void handleRecordingFailure(const QString& reason);
    void handleStreamingFailure(const QString& reason);
};

#endif // CAMERA_H
