#ifndef CAMERA_STRUCTS_H
#define CAMERA_STRUCTS_H

#include <QMutex>
#include <QString>
#include <QWindow>
#include <deque>
#include <opencv2/opencv.hpp>
#include "MvCameraControl.h"

class CameraWorker;
class FrameProcessor;
class VideoRecorder;
class VideoStreamer;

// Структура для хранения информации о камере
struct CameraFrameInfo {
    QString name;                     // Имя камеры (LCamera, RCamera, UCamera, DCamera и т.д.)
    unsigned int id = -1;             // ID камеры в списке устройств
    void* handle = nullptr;           // Дескриптор камеры
    MV_DISPLAY_FRAME_INFO frame;      // Данные кадра
    QMutex* mutex = nullptr;          // Указатель на мьютекс для синхронизации
    CameraWorker* worker = nullptr;   // Рабочий объект для захвата
    QThread* thread = nullptr;        // Поток для захвата
    WId labelWinId = 0;               // Дескриптор окна для отображения

    CameraFrameInfo() {
        mutex = new QMutex();
    }

    ~CameraFrameInfo() {
        delete mutex;
    }
};

// Структура для хранения данных кадра и записи видео
struct CameraVideoFrameInfo {
    QString name;                     // Имя камеры (LCamera, RCamera, UCamera, DCamera и т.д.)
    unsigned int id = -1;             // ID камеры в списке устройств
    void* handle = nullptr;           // Дескриптор камеры
    QMutex* mutex = nullptr;          // Указатель на мьютекс для синхронизации
    std::deque<cv::Mat> frameBuffer;  // Буфер для хранения 5 кадров
    int bufferIndex = 0;              // Индекс для циклического доступа
    FrameProcessor* processor = nullptr; // Объект для обработки кадров
    QThread* processorThread = nullptr;  // Поток для обработки кадров
    VideoRecorder* recorder = nullptr;   // Объект для записи видео
    QThread* recorderThread = nullptr;   // Поток для записи видео
    VideoStreamer* streamer = nullptr;   // Объект для стриминга видео
    QThread* streamerThread = nullptr;   // Поток для стриминга видео

    CameraVideoFrameInfo() {
        mutex = new QMutex();
        frameBuffer.resize(5); // Инициализируем буфер размером 5
    }

    ~CameraVideoFrameInfo() {
        delete mutex;
    }
};

#endif // CAMERA_STRUCTS_H
