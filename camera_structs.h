#ifndef CAMERA_STRUCTS_H
#define CAMERA_STRUCTS_H

#include <QMutex>
#include <QString>
#include <QWindow>
#include <deque>
#include <opencv2/opencv.hpp>
#include "MvCameraControl.h"
#include <filesystem>

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
    QImage img;

    CameraFrameInfo() {
        mutex = new QMutex();
    }

    ~CameraFrameInfo() {
        delete mutex;
    }
};

// Структура для стриминга видео
struct StreamFrameInfo {
    QString name;                     // Имя камеры
    unsigned int id = -1;             // ID камеры в списке устройств
    cv::Mat img;                      // Текущий кадр для стриминга
    QMutex* mutex = nullptr;          // Указатель на мьютекс для синхронизации
    VideoStreamer* streamer = nullptr; // Объект для стриминга видео
    QThread* streamerThread = nullptr; // Поток для стриминга видео

    StreamFrameInfo() {
        mutex = new QMutex();
    }

    ~StreamFrameInfo() {
        delete mutex;
    }
};

// Структура для записи видео
struct RecordFrameInfo {
    QString name;                     // Имя камеры
    unsigned int id = -1;             // ID камеры в списке устройств
    cv::Mat img;                      // Текущий кадр для записи
    QMutex* mutex = nullptr;          // Указатель на мьютекс для синхронизации
    VideoRecorder* recorder = nullptr; // Объект для записи видео
    QThread* recorderThread = nullptr; // Поток для записи видео
    std::filesystem::path sessionDirectory; // Путь к сессионной папке

    RecordFrameInfo() {
        mutex = new QMutex();
    }

    ~RecordFrameInfo() {
        delete mutex;
    }
};


#endif // CAMERA_STRUCTS_H
