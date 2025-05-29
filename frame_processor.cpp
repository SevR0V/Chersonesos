#include "frame_processor.h"

FrameProcessor::FrameProcessor(CameraVideoFrameInfo* videoInfo, QObject* parent)
    : QObject(parent), m_videoInfo(videoInfo) {}

void FrameProcessor::processFrame(const QByteArray& frameData, int width, int height, int pixelType) {
    try {
        // Создаём cv::Mat из полученных данных
        cv::Mat rawFrame;
        if (pixelType == PixelType_Gvsp_BayerRG8) {
            // Данные в формате BayerRG8
            cv::Mat bayerFrame(height, width, CV_8UC1, const_cast<char*>(frameData.data()), width);
            rawFrame = cv::Mat(height, width, CV_8UC3);
            cv::cvtColor(bayerFrame, rawFrame, cv::COLOR_BayerRG2RGB);
        } else {
            QString errorMsg = QString("Неподдерживаемый формат пикселей для камеры %1: pixelType = %2")
                                   .arg(m_videoInfo->name).arg(pixelType);
            qDebug() << errorMsg;
            emit errorOccurred("FrameProcessor", errorMsg);
            return;
        }

        // Обновляем буфер
        QMutexLocker locker(m_videoInfo->mutex);
        m_videoInfo->frameBuffer[m_videoInfo->bufferIndex] = rawFrame.clone();
        m_videoInfo->bufferIndex = (m_videoInfo->bufferIndex + 1) % 5;
    } catch (const cv::Exception& e) {
        QString errorMsg = QString("Ошибка преобразования в cv::Mat для камеры %1: %2").arg(m_videoInfo->name).arg(e.what());
        qDebug() << errorMsg;
        emit errorOccurred("FrameProcessor", errorMsg);
    }
}
