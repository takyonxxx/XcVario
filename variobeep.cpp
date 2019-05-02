#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QDebug>
#include <QVBoxLayout>
#include <qmath.h>
#include <qendian.h>
#include <QThread>
#include <variobeep.h>

#define PUSH_MODE_LABEL "Enable push mode"
#define PULL_MODE_LABEL "Enable pull mode"
#define SUSPEND_LABEL   "Suspend playback"
#define RESUME_LABEL    "Resume playback"
#define VOLUME_LABEL    "Volume:"

const int DataSampleRateHz = 44100;
const int BufferSize      = 32768;


VarioBeep::VarioBeep(int ToneSampleRateHz,int DurationUSeconds, QObject *parent)
    :   QObject (parent)
    ,   m_device(QAudioDeviceInfo::defaultOutputDevice())
    ,   m_generator(nullptr)
    ,   m_audioOutput(nullptr)
    ,   m_buffer(BufferSize, 0)
    ,   m_vario(0.0)
    ,   m_tone(0.0)
    ,   m_outputVolume(0.0)
    ,   m_running(false)
    ,   m_toneSampleRateHz(ToneSampleRateHz)
    ,   m_durationUSeconds(DurationUSeconds)
{
    m_varioFunction = new PiecewiseLinearFunction();
    m_varioFunction->addNewPoint(QPointF(0, 0.4763));
    m_varioFunction->addNewPoint(QPointF(0.441, 0.3619));
    m_varioFunction->addNewPoint(QPointF(1.029, 0.2238));
    m_varioFunction->addNewPoint(QPointF(1.559, 0.1565));
    m_varioFunction->addNewPoint(QPointF(2.471, 0.0985));
    m_varioFunction->addNewPoint(QPointF(3.571, 0.0741));
    m_varioFunction->addNewPoint(QPointF(5.0, 0.05));

    m_toneFunction = new PiecewiseLinearFunction();
    m_toneFunction->addNewPoint(QPointF(0, m_toneSampleRateHz));
    m_toneFunction->addNewPoint(QPointF(0.25, m_toneSampleRateHz + 100));
    m_toneFunction->addNewPoint(QPointF(1.0, m_toneSampleRateHz + 200));
    m_toneFunction->addNewPoint(QPointF(1.5, m_toneSampleRateHz + 300));
    m_toneFunction->addNewPoint(QPointF(2.0, m_toneSampleRateHz + 400));
    m_toneFunction->addNewPoint(QPointF(3.5, m_toneSampleRateHz + 500));
    m_toneFunction->addNewPoint(QPointF(4.0, m_toneSampleRateHz + 600));
    m_toneFunction->addNewPoint(QPointF(4.5, m_toneSampleRateHz + 700));
    m_toneFunction->addNewPoint(QPointF(6.0, m_toneSampleRateHz + 800));

    initializeAudio();
}

void VarioBeep::initializeAudio()
{     
    m_format.setSampleRate(DataSampleRateHz);
    m_format.setChannelCount(1);
    m_format.setSampleSize(16);
    m_format.setCodec("audio/pcm");
    m_format.setByteOrder(QAudioFormat::LittleEndian);
    m_format.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo info(m_device);
    if (!info.isFormatSupported(m_format)) {
        qWarning() << "Default format not supported - trying to use nearest";
        m_format = info.nearestFormat(m_format);
    }

    createAudioOutput();
}

void VarioBeep::createAudioOutput()
{
    delete m_audioOutput;
    m_audioOutput = nullptr;
    m_audioOutput = new QAudioOutput(m_device, m_format, this);
}

void VarioBeep::startBeep()
{
    if(timerID > 0)
    {
        killTimer(timerID);
        this->timerID = 0;
    }

    timerID = startTimer(0);
    SetFrequency(m_toneSampleRateHz);
    m_running = true;
}

void VarioBeep::stopBeep()
{   
    if(timerID > 0)
    {
        killTimer(timerID);
        this->timerID = 0;
    }

    m_running = false;
    m_audioOutput->stop();
}

void VarioBeep::resumeBeep()
{
    m_audioOutput->resume();
}

void VarioBeep::setVolume(qreal volume)
{
    m_audioOutput->setVolume(static_cast<qreal>(volume / 100));
}

void VarioBeep::SetVario(qreal vario)
{
    m_vario = vario;
}

void VarioBeep::SetFrequency(int freq)
{
    delete m_generator;
    m_generator = new Generator(m_format, static_cast<qint64>(m_varioFunction->getValue(m_vario) * 1000), freq, this);
    m_generator->start();
}

void VarioBeep::timerEvent(QTimerEvent *event)
{
    if(m_running)
    {
        if(m_vario > 0)
        {
            m_tone = static_cast<int>(m_toneFunction->getValue(m_vario));
        }
        else if(m_vario < 0)
        {
            m_tone = m_toneSampleRateHz;
        }

        if(m_vario < 0.25) return;

        delete m_generator;
        m_generator = new Generator(m_format, static_cast<qint64>(m_varioFunction->getValue(m_vario) * 1000), m_tone, this);
        m_generator->start();

        m_audioOutput->start(m_generator);
        Sleeper::msleep(static_cast<unsigned>(m_varioFunction->getValue(m_vario) * 1000));
        m_audioOutput->suspend();
        Sleeper::msleep(static_cast<unsigned>(m_varioFunction->getValue(m_vario) * 500));
    }
}

VarioBeep::~VarioBeep()
{   
    delete m_generator;
}
