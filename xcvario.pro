QT += core gui quick sensors positioning multimedia widgets network

win{
QT += serialport
}

TARGET = xcvario
TEMPLATE = app


SOURCES += main.cpp\
    kalmanfilter.cpp \
    logindialog.cpp \
    mainwindow.cpp \
    networkaccessmanager.cpp \
    variobeep.cpp \
    generator.cpp \
    piecewiselinearfunction.cpp

HEADERS  += \
    kalmanfilter.h \
    logindialog.h \
    mainwindow.h \
    networkaccessmanager.h \
    variobeep.h \
    generator.h \
    piecewiselinearfunction.h

FORMS    += \
    mainwindow.ui

RESOURCES += \
    resources.qrc

win:RC_ICONS += $$PWD\appIcon.ico

ios {
QMAKE_INFO_PLIST = ios/Info.plist
}
