QT += core gui quick sensors positioning multimedia widgets
QT += serialport network

TARGET = xcvario
TEMPLATE = app


SOURCES += main.cpp\
    kalmanfilter.cpp \
    mainwindow.cpp \
    networkaccessmanager.cpp \
    variobeep.cpp \
    generator.cpp \
    piecewiselinearfunction.cpp

HEADERS  += \
    kalmanfilter.h \
    mainwindow.h \
    networkaccessmanager.h \
    variobeep.h \
    generator.h \
    piecewiselinearfunction.h

FORMS    += \
    mainwindow.ui

RESOURCES += \
    resources.qrc

win32:RC_ICONS += $$PWD\appIcon.ico

ios {
QMAKE_INFO_PLIST = ios/Info.plist
}
