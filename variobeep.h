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

class Sleeper
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
    VarioBeep(int ToneSampleRateHz, int DurationUSeconds);
    ~VarioBeep();

    qreal EPSILON = 0.0001;

    bool AreSame(qreal a, qreal b)
    {
        return fabs(a - b) < EPSILON;
    }

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
