#include "camera_worker.h"

CameraWorker::CameraWorker(CameraFrameInfo* frameInfo, CameraVideoFrameInfo* videoInfo, QObject* parent)
    : QObject(parent), m_frameInfo(frameInfo), m_videoInfo(videoInfo), m_isRunning(true) {
    qDebug() << "Создан CameraWorker для камеры" << m_frameInfo->name;
}

CameraWorker::~CameraWorker() {
    cleanupCamera();
}

void CameraWorker::stop() {
    m_isRunning = false;
    qDebug() << "Остановка CameraWorker для камеры" << m_frameInfo->name;
}

void CameraWorker::capture() {
    qDebug() << "Начало потока захвата для камеры" << m_frameInfo->name;

    if (!m_frameInfo->handle) {
        QString errorMsg = QString("Недействительный дескриптор камеры %1").arg(m_frameInfo->name);
        qDebug() << errorMsg;
        emit errorOccurred("CameraWorker", errorMsg);
        emit captureFailed(errorMsg);
        return;
    }

    if (m_frameInfo->labelWinId == 0) {
        qDebug() << "Предупреждение: labelWinId не установлен для камеры" << m_frameInfo->name;
    }

    int nRet = MV_OK;

    // Запуск захвата
    nRet = MV_CC_StartGrabbing(m_frameInfo->handle);
    if (nRet != MV_OK) {
        QString errorMsg = QString("Не удалось запустить захват для камеры %1. Ошибка: %2")
                               .arg(m_frameInfo->name).arg(nRet);
        qDebug() << errorMsg;
        cleanupCamera();
        emit errorOccurred("CameraWorker", errorMsg);
        emit captureFailed(errorMsg);
        return;
    }
    QThread::msleep(100);

    // Основной цикл захвата
    MV_FRAME_OUT stOutFrame = {0};
    int retryCount = 5;
    while (m_isRunning) {
        if (!m_isRunning) break;

        nRet = MV_CC_GetImageBuffer(m_frameInfo->handle, &stOutFrame, 500);
        if (nRet == MV_OK && stOutFrame.pBufAddr) {
            {
                QMutexLocker locker(m_frameInfo->mutex);
                m_frameInfo->frame.pData = stOutFrame.pBufAddr;
                m_frameInfo->frame.nWidth = stOutFrame.stFrameInfo.nWidth;
                m_frameInfo->frame.nHeight = stOutFrame.stFrameInfo.nHeight;
                m_frameInfo->frame.enPixelType = stOutFrame.stFrameInfo.enPixelType;
                m_frameInfo->frame.nDataLen = stOutFrame.stFrameInfo.nWidth * stOutFrame.stFrameInfo.nHeight * 3;
                m_frameInfo->frame.enRenderMode = 0;

                //if (m_frameInfo->labelWinId != 0) {
                    //nRet = MV_CC_DisplayOneFrame(m_frameInfo->handle, &m_frameInfo->frame);
                //}

                cv::Mat bayerMat(stOutFrame.stFrameInfo.nHeight, stOutFrame.stFrameInfo.nWidth, CV_8UC1, stOutFrame.pBufAddr);

                cv::cvtColor(bayerMat, bayerMat, cv::COLOR_BayerRG2BGR);

                m_frameInfo->img = QImage(bayerMat.data, bayerMat.cols, bayerMat.rows, bayerMat.step, QImage::Format_RGB888).copy();

            }

            if (stOutFrame.pBufAddr && stOutFrame.stFrameInfo.nWidth > 0 && stOutFrame.stFrameInfo.nHeight > 0) {
                QByteArray frameData(reinterpret_cast<const char*>(stOutFrame.pBufAddr), stOutFrame.stFrameInfo.nFrameLen);
                emit frameReady();
                emit frameDataReady(frameData, stOutFrame.stFrameInfo.nWidth, stOutFrame.stFrameInfo.nHeight, stOutFrame.stFrameInfo.enPixelType);
            } else {
                QString errorMsg = QString("Получены некорректные данные кадра для камеры %1: pData=%2, ширина=%3, высота=%4")
                                       .arg(m_frameInfo->name)
                                       .arg((quintptr)stOutFrame.pBufAddr, 0, 16)
                                       .arg(stOutFrame.stFrameInfo.nWidth)
                                       .arg(stOutFrame.stFrameInfo.nHeight);
                qDebug() << errorMsg;
                emit errorOccurred("CameraWorker", errorMsg);
            }

            MV_CC_FreeImageBuffer(m_frameInfo->handle, &stOutFrame);
            retryCount = 5;
        } else {
            if (nRet == -2147483641) {
                retryCount--;
                if (retryCount > 0) {
                    qDebug() << "Не удалось получить данные для камеры" << m_frameInfo->name
                             << ", Осталось попыток:" << retryCount;
                    QThread::msleep(100);
                    continue;
                }
            }
            QString errorMsg = QString("Не удалось получить буфер изображения для камеры %1. Ошибка: %2")
                                   .arg(m_frameInfo->name).arg(nRet);
            qDebug() << errorMsg;
            MV_CC_FreeImageBuffer(m_frameInfo->handle, &stOutFrame);
            cleanupCamera();
            emit errorOccurred("CameraWorker", errorMsg);
            emit captureFailed(errorMsg);
            return;
        }
    }

    qDebug() << "Конец потока захвата для камеры" << m_frameInfo->name;
    if (m_frameInfo->handle) {
        MV_CC_StopGrabbing(m_frameInfo->handle);
    }

    cleanupCamera();
}

void CameraWorker::cleanupCamera() {
    if (m_frameInfo->handle) {
        int nRet = MV_OK;
        nRet = MV_CC_StopGrabbing(m_frameInfo->handle);
        if (nRet != MV_OK && nRet != -2147483648 && nRet != -2147483645 && nRet != -2147483133) {
            QString errorMsg = QString("Не удалось остановить захват для камеры %1. Ошибка: %2")
                                   .arg(m_frameInfo->name).arg(nRet);
            qDebug() << errorMsg;
            emit errorOccurred("CameraWorker", errorMsg);
        }
        nRet = MV_CC_CloseDevice(m_frameInfo->handle);
        if (nRet != MV_OK && nRet != -2147483648 && nRet != -2147483645) {
            QString errorMsg = QString("Не удалось закрыть устройство для камеры %1. Ошибка: %2")
                                   .arg(m_frameInfo->name).arg(nRet);
            qDebug() << errorMsg;
            emit errorOccurred("CameraWorker", errorMsg);
        }
        nRet = MV_CC_DestroyHandle(m_frameInfo->handle);
        if (nRet != MV_OK && nRet != -2147483648) {
            QString errorMsg = QString("Не удалось уничтожить дескриптор для камеры %1. Ошибка: %2")
                                   .arg(m_frameInfo->name).arg(nRet);
            qDebug() << errorMsg;
            emit errorOccurred("CameraWorker", errorMsg);
        }
        m_frameInfo->handle = nullptr;
        QThread::msleep(3000);
        qDebug() << "Ресурсы камеры очищены для" << m_frameInfo->name;
    }
}
