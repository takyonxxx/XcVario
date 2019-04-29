QT += core gui sensors positioning multimedia widgets concurrent

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
