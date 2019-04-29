QT += core gui sensors positioning multimedia widgets

TARGET = xcvario
TEMPLATE = app


SOURCES += main.cpp\
    kalmanfilter.cpp \
    mainwindow.cpp \
    variobeep.cpp \
    generator.cpp \
    piecewiselinearfunction.cpp

HEADERS  += \
    kalmanfilter.h \
    mainwindow.h \
    variobeep.h \
    generator.h \
    piecewiselinearfunction.h

FORMS    += \
    mainwindow.ui

RESOURCES += \
    resources.qrc

win32:RC_ICONS += $$PWD\appIcon.ico
