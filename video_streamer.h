#ifndef VIDEO_STREAMER_H
#define VIDEO_STREAMER_H

#include <QObject>
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSet>
#include <QThread>
#include <opencv2/opencv.hpp>
#include "camera_structs.h"

class VideoStreamer : public QObject {
    Q_OBJECT
public:
    explicit VideoStreamer(CameraVideoFrameInfo* videoInfo, int port, QObject* parent = nullptr);
    ~VideoStreamer();

public slots:
    void startStreaming();
    void stopStreaming();

private slots:
    void closeAllConnections();
    void handleNewConnection();

private:
    void sendMJPEGHeader(QTcpSocket* client);
    void streamFrames(QTcpSocket* client);

signals:
    void streamingStarted();
    void streamingFinished();
    void streamingFailed(const QString& reason);
    void errorOccurred(const QString& component, const QString& message);

private:
    CameraVideoFrameInfo* m_videoInfo;
    int m_port;
    QTcpServer* m_server;
    QSet<QTcpSocket*> m_clients;
    bool m_isStreaming;
};

#endif // VIDEO_STREAMER_H
