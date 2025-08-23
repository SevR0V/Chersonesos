#include "camera_worker.h"

CameraWorker::CameraWorker(CameraFrameInfo* frameInfo, StreamFrameInfo* streamInfo, RecordFrameInfo* recordInfo, QObject* parent)
    : QObject(parent), m_frameInfo(frameInfo), m_streamInfo(streamInfo), m_recordInfo(recordInfo), m_isRunning(true) {
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
                cv::Mat bayerMat(stOutFrame.stFrameInfo.nHeight, stOutFrame.stFrameInfo.nWidth, CV_8UC1, stOutFrame.pBufAddr);
                cv::Mat rawFrame(stOutFrame.stFrameInfo.nHeight, stOutFrame.stFrameInfo.nWidth, CV_8UC3);
                cv::cvtColor(bayerMat, rawFrame, cv::COLOR_BayerRG2BGR);
                QImage qimg = QImage(rawFrame.data, rawFrame.cols, rawFrame.rows, rawFrame.step, QImage::Format_RGB888).copy();

                // Для CameraFrameInfo (QImage)
                {
                    QMutexLocker locker(m_frameInfo->mutex);
                    int backIndex = 1 - m_frameInfo->frontIndex.load(std::memory_order_relaxed);  // Вычисляем back
                    m_frameInfo->buffers[backIndex] = qimg;  // Пишем в back
                    m_frameInfo->frontIndex.store(backIndex, std::memory_order_release);  // Atomic swap
                }

                // Для StreamFrameInfo (cv::Mat)
                {
                    QMutexLocker locker(m_streamInfo->mutex);
                    int backIndex = 1 - m_streamInfo->frontIndex.load(std::memory_order_relaxed);
                    m_streamInfo->buffers[backIndex] = rawFrame.clone();  // Пишем в back (clone для safety)
                    m_streamInfo->frontIndex.store(backIndex, std::memory_order_release);  // Swap
                }

                // Для RecordFrameInfo (cv::Mat)
                {
                    QMutexLocker locker(m_recordInfo->mutex);
                    int backIndex = 1 - m_recordInfo->frontIndex.load(std::memory_order_relaxed);
                    m_recordInfo->buffers[backIndex] = rawFrame.clone();  // Пишем в back
                    m_recordInfo->frontIndex.store(backIndex, std::memory_order_release);  // Swap
                }
            }

            if (stOutFrame.pBufAddr && stOutFrame.stFrameInfo.nWidth > 0 && stOutFrame.stFrameInfo.nHeight > 0) {
                emit frameReady();
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
                    QThread::msleep(33);
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
