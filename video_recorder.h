#ifndef VIDEO_RECORDER_H
#define VIDEO_RECORDER_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <filesystem>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include "camera_structs.h"

class VideoRecorder : public QObject {
    Q_OBJECT
public:
    explicit VideoRecorder(RecordFrameInfo* recordInfo, QObject* parent = nullptr);
    void setRecordInterval(int interval);
    void setStoredVideoFilesLimit(int limit);

public slots:
    void startRecording();
    void stopRecording();
    void recordFrame();

signals:
    void recordingStarted();
    void recordingFinished();
    void recordingFailed(const QString& reason);
    void errorOccurred(const QString& component, const QString& message);

private:
    void manageStoredFiles();
    void startNewSegment();
    std::string sanitizeFileName(const std::string& input);
    std::string generateFileName(const std::string& prefix, const std::string& extension);
    std::string generateDateDirectoryName();
    std::string generateTimeDirectoryName();
    RecordFrameInfo* m_recordInfo;
    cv::VideoWriter videoWriter;
    QElapsedTimer m_timer;
    std::string fileName;
    bool m_isRecording;
    int m_recordInterval;
    int m_storedVideoFilesLimit;
    std::filesystem::path m_sessionDirectory;
};

#endif // VIDEO_RECORDER_H
