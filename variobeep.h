/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <generator.h>

#include <QAudioOutput>
#include <QByteArray>
#include <QIODevice>
#include <QLabel>
#include <QMainWindow>
#include <QObject>
#include <QPushButton>
#include <QFuture>
#include <QtConcurrent>
#include <piecewiselinearfunction.h>

class Sleeper : public QThread
{
public:
    static void usleep(unsigned long usecs){QThread::usleep(usecs);}
    static void msleep(unsigned long msecs){QThread::msleep(msecs);}
    static void sleep(unsigned long secs){QThread::sleep(secs);}
};

class VarioBeep : public QMainWindow
{
    Q_OBJECT

public:
    VarioBeep(qreal ToneSampleRateHz, int DurationUSeconds);
    ~VarioBeep();

    void startBeep();
    void stopBeep();       
    void resumeBeep();
    void setVolume(qreal volume);  
    void SetVario(qreal vario, qreal tdiff);
    PiecewiseLinearFunction *m_varioFunction;
    PiecewiseLinearFunction *m_toneFunction;

private:      
    void varioThread();
    void initializeAudio();
    void createAudioOutput();
    void SetFrequency(int freq);

private:     
    QAudioDeviceInfo m_device;   
    Generator *m_generator;
    Generator* tmp;
    QAudioOutput *m_audioOutput;   
    QAudioFormat m_format;
    QByteArray m_buffer;         
    qreal m_vario;
    int m_tone;
    qreal m_tdiff;
    qreal m_outputVolume;
    bool m_running;
    int m_toneSampleRateHz;
    int m_durationUSeconds;
    QFuture<void> futureVario;
};

#endif // AUDIOOUTPUT_H
