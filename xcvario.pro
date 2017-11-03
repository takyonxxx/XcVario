QT       += core gui sensors positioning multimedia widgets concurrent
QT += androidextras

TARGET = xcvario
TEMPLATE = app


SOURCES += main.cpp\
        widget.cpp \
    kalmanfilter.cpp \
    variobeep.cpp \
    generator.cpp \
    piecewiselinearfunction.cpp

HEADERS  += widget.h \
    kalmanfilter.h \
    variobeep.h \
    generator.h \
    piecewiselinearfunction.h

FORMS    += widget.ui

DISTFILES += \
    android-sources/AndroidManifest.xml

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android-sources
winrt: WINRT_MANIFEST.capabilities_device += location


