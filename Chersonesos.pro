QT += core gui openglwidgets network concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_PROJECT_DEPTH = 0

CONFIG += c++17
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    SettingsManager.cpp \
    controlwindow.cpp \
    iplineedit.cpp \
    lineeditutils.cpp \
    customlineedit.cpp \
    gamepadworker.cpp \
    mainwindow.cpp \
    profilemanager.cpp \
    camera.cpp \
    camera_worker.cpp \
    logger.cpp \
    main.cpp \
    settingsdialog.cpp \
    udphandler.cpp \
    udptelemetryparser.cpp \
    video_recorder.cpp \
    video_streamer.cpp \
    overlaywidget.cpp

HEADERS += \
    SettingsManager.h \
    controlwindow.h \
    customlineedit.h \
    gamepadworker.h \
    iplineedit.h \
    lineeditutils.h \
    mainwindow.h \
    profilemanager.h \
    camera.h \
    camera_structs.h \
    camera_worker.h \
    logger.h \
    settingsdialog.h \
    udphandler.h \
    udptelemetryparser.h \
    video_recorder.h \
    video_streamer.h \
    overlaywidget.h

FORMS += \
    controlwindow.ui \
    mainwindow.ui \
    settingsdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += c:\opencv-4.10.0-build\install\include
LIBS += -lwsock32
LIBS += -lws2_32
LIBS += -LC:\opencv-4.10.0-build\install\x64\vc17\lib
LIBS += -lopencv_core4100 -lopencv_imgcodecs4100 -lopencv_highgui4100 -lopencv_features2d4100 -lopencv_calib3d4100 -lopencv_videoio4100 -lopencv_imgproc4100 -lopencv_ximgproc4100

LIBS += -LC:\MVS\Development\Libraries\win64 -lMvCameraControl
INCLUDEPATH += c:\MVS\Development\Includes

unix|win32: LIBS += -L$$PWD/SDL3/lib/x64/ -lSDL3

INCLUDEPATH += $$PWD/SDL3/include
DEPENDPATH += $$PWD/SDL3/include

CONFIG(release, debug|release) {
    QMAKE_POST_LINK += $$quote($$[QT_INSTALL_BINS]/windeployqt.exe $$OUT_PWD/release/$${TARGET}.exe)
} else {
    QMAKE_POST_LINK += $$quote($$[QT_INSTALL_BINS]/windeployqt.exe $$OUT_PWD/debug/$${TARGET}.exe)
}

RESOURCES += \
    resources.qrc
