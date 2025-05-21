QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    controlwindow.cpp \
    main.cpp \
    customlineedit.cpp \
    gamepadworker.cpp \
    mainwindow.cpp \
    profilemanager.cpp

HEADERS += \
    controlwindow.h \
    customlineedit.h \
    gamepadworker.h \
    mainwindow.h \
    profilemanager.h

FORMS += \
    controlwindow.ui \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

unix|win32: LIBS += -L$$PWD/SDL3/lib/x64/ -lSDL3

INCLUDEPATH += $$PWD/SDL3/include
DEPENDPATH += $$PWD/SDL3/include

CONFIG(release, debug|release) {
    QMAKE_POST_LINK += $$quote($$[QT_INSTALL_BINS]/windeployqt.exe $$OUT_PWD/release/$${TARGET}.exe)
} else {
    QMAKE_POST_LINK += $$quote($$[QT_INSTALL_BINS]/windeployqt.exe $$OUT_PWD/debug/$${TARGET}.exe)
}
