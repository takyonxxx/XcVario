#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_running(false),
    createIgcFile(false),
    pressure (101325.0),
    altitude (0),
    oldaltitude(0),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("XcVario");

    path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QString("/VarioLog/");
    QDir dir;

    // We create the directory if needed
    if (!dir.exists(path))
        dir.mkpath(path); // You can check the success if needed

    ui->label_vario->setStyleSheet("font-size: 16pt; color: #cccccc; background-color: #001a1a;");
    ui->label_gps->setStyleSheet("font-size: 16pt; color: #cccccc; background-color: #001a1a;");

    varioBeep = new VarioBeep(750.0, static_cast<int>(DURATION_MS * 1000), this);
    varioBeep->setVolume(100);

    //Sensors
    (void)QSensor::sensorTypes();
    QSensor *sensor = new QSensor(QByteArray(), this);
    connect(sensor, SIGNAL(availableSensorsChanged()), this, SLOT(loadSensors()));

    //Gps  
    m_posSource = QGeoPositionInfoSource::createDefaultSource(this);
    if (!m_posSource)
        qFatal("No Gps Position Source created!");
    else
    {
        connect(m_posSource, SIGNAL(positionUpdated(QGeoPositionInfo)),
                this, SLOT(positionUpdated(QGeoPositionInfo)));

        connect(m_posSource, SIGNAL(error(QGeoPositionInfoSource::Error)),
                this, SLOT(errorChanged(QGeoPositionInfoSource::Error)));

        setInterval(0);
        QString availableSources;
        availableSources.append("Available gps sources:\n");
        QStringList posSourcesList = QGeoPositionInfoSource::availableSources();

        foreach (const QString &src, posSourcesList) {           
            availableSources.append(src + "\n");
        }
        ui->label_gps->setText(availableSources);
    }
}

MainWindow::~MainWindow()
{
    if(igcFile->isOpen())
    {
        igcFile->close();
    }

    delete ui;
}

void MainWindow::loadSensors()
{
    QString status;
    bool found = false;
    status.append("Searching sensors...");
    status.append("\n");
    int count = 1;

    foreach (const QByteArray &type, QSensor::sensorTypes()) {
        foreach (const QByteArray &identifier, QSensor::sensorsForType(type)) {
            // Don't put in sensors we can't connect to
            QSensor sensor(type);
            sensor.setIdentifier(identifier);
            if (!sensor.connectToBackend()) {
                status.append("Couldn't connect to " + identifier + "\n");
                continue;
            }

            QString sensor_type = type;
            status.append(QString::number(count) + " - " + sensor_type + "\n");
            if(sensor_type.contains("Pressure"))
            {
                m_sensor = new QPressureSensor(this);
                connect(m_sensor, SIGNAL(readingChanged()), this, SLOT(sensor_changed()));
                m_sensor->setIdentifier(identifier);
                if (!m_sensor->connectToBackend()) {
                    status.append("Can't connect to Backend sensor: " + sensor_type + "\n");
                    delete m_sensor;
                    m_sensor = nullptr;
                    return;
                }
                found = true;
                if(m_sensor->start())
                {
                    status.append(sensor_type + " started succesfully.\n");
                    start = QDateTime::currentDateTime();
                    pressure_filter = new KalmanFilter(KF_VAR_ACCEL);
                    pressure_filter->Reset(pressure);
                    altitude_filter = new KalmanFilter(KF_VAR_ACCEL);
                    altitude_filter->Reset(altitude);
                }
            }
            count++;
        }
    }
    if(!found)
        status.append("Pressure sensor could not found.\n");

    ui->label_vario->setText(status);
}

void MainWindow::showEvent(QShowEvent *event)
{
    // Once we're visible, load the sensors
    // (don't delay showing the UI while we load plugins, connect to backends, etc.)
    QTimer::singleShot(0, this, SLOT(loadSensors()));
    QWidget::showEvent(event);
}

void MainWindow::sensor_changed()
{
    end = QDateTime::currentDateTime();
    pressure_reading = m_sensor->reading();
    dt = start.msecsTo(end) / 1000.;

    if(pressure_reading != nullptr)
    {
        pressure = pressure_reading->pressure();
        temperature = pressure_reading->temperature();
        //(pressure, KF_VAR_MEASUREMENT, millisecondsDiff);

        pressure_filter->Update(pressure,KF_VAR_MEASUREMENT,dt);
        pressure = pressure_filter->GetXAbs();

        auto ratioPS = static_cast<float>(pressure / sealevel);

        baroaltitude = 44330.0 * (1.0 - static_cast<qreal>(powf(static_cast<float>(ratioPS), static_cast<float>(0.19f))));
        altitude_filter->Update(baroaltitude, KF_VAR_MEASUREMENT, dt);
        altitude = altitude_filter->GetXAbs();
        vario = altitude_filter->GetXVel();
        varioBeep->SetVario(vario, dt);

        fillVario(vario);
    }
    else
    {
        text_presssure = "\nSensor: UNAVAILABLE";
    }
    oldaltitude = altitude;
    start = end;
}

void MainWindow::fillVario(qreal vario)
{
    ui->label_vario->setText(
                "<span style='font-size:110pt; font-weight:600;'>"
                + QString::number(vario, 'f', 1) +"</span>"
                + "<span style='font-size:36pt; font-weight:600; color:#00cccc;'> m/s</span>"
                );

}

void MainWindow::positionUpdated(QGeoPositionInfo gpsPos)
{
    //if(!m_coord.isValid())return;

    m_gpsPos = gpsPos;
    m_coord = gpsPos.coordinate();
    auto timestamp = gpsPos.timestamp();
    auto local = timestamp.toLocalTime();
    auto gps_altitude = m_coord.altitude();
    auto dateTimeString = local.toString("hh : mm : ss");
    text_igc_name = "VarioLog_" + local.toString("dd_MM_yyyy__hh_mm_ss") + ".igc";
    //ui->label_time->setText(dateTimeString + "\nAcc: " + QString::number(horizontalAccuracy, 'f', 1) + " m");
    ui->label_gps->setText(
                "<span style='font-size:22pt; font-weight:600;color:#ff6600;'>"
                + dateTimeString + "</span>" + "<br />"
                + "<span style='font-size:22pt; font-weight:600; color:white;'>Altitude: "
                + QString::number(gps_altitude, 'f', 1) + " m</span>" + "<br />"
                + "<span style='font-size:22pt; font-weight:600; color:white;'>BaroAlt: "
                + QString::number(altitude, 'f', 1) + " m</span>"
                );

    updateIGC();
}

void MainWindow::errorChanged(QGeoPositionInfoSource::Error err)
{
    qDebug() << (err == 3 ? QStringLiteral("No error") : QString::number(err));
}

void MainWindow::setInterval(int msec)
{
    m_posSource->setUpdateInterval(msec);
}

//gps
void MainWindow::updateIGC()
{
    if(!createIgcFile)
    {
        QString sdate = QDate::currentDate().toString("ddMMyy");
        QString stime = QTime::currentTime().toString("hhmmss");

        createIgcHeader();
        createIgcFile = true;
    }
    else
    {
        QDateTime timestamp = m_gpsPos.timestamp();
        //QDateTime local = timestamp.toLocalTime();
        QString dateTimeString = timestamp.toString("hhmmss");

        igcFile->open(QIODevice::Append | QIODevice::Text);

        if(!igcFile->isOpen()){
            qDebug() << "- Error, unable to open" << "outputFilename" << "for output";
        }

        QTextStream out(igcFile);
        QString str;

        //B,110135,5206343N,00006198W,A,00587,00558

        QString record  = "B";

        record.append(dateTimeString);
        record.append(decimalToDDDMMMMMLat(m_coord.latitude()));
        record.append(decimalToDDDMMMMMLon(m_coord.longitude()));
        record.append("A");
        str.sprintf("%05d",static_cast<int>(m_coord.altitude()));
        record.append(str);
        str.sprintf("%05d",static_cast<int>(altitude));
        record.append(str);
        out << record << endl;

        igcFile->close();
    }
}

void MainWindow::createIgcHeader()
{
    igcFile = new QFile();
    igcFile->setFileName(path + text_igc_name);
    qDebug() << "Igc path: " << path;

    igcFile->open(QIODevice::Append | QIODevice::Text);

    if(!igcFile->isOpen()){
        qDebug() << "- Error, unable to open" << "outputFilename" << "for output";
    }

    QTextStream out(igcFile);

    QDateTime timestamp = m_gpsPos.timestamp();
    QString dateString = timestamp.toString("ddMMyy");

    QString header  = "AXGD000 XcVario v1.0\n";
    header.append("HFDTE" + dateString + "\n");
    header.append("HODTM100GPSDATUM: WGS-84\n");
    header.append("HFFTYFRTYPE: XcVario by Türkay Biliyor");
    out << header << endl;
    igcFile->close();
    createIgcFile = true;
}

QString MainWindow::decimalToDDDMMMMMLat(double angle)
{
    QString output, strdegree, strminutes, hemisphere;

    if (angle < 0) {
        angle = -1 * angle;
        hemisphere = "S";
    } else {
        hemisphere = "N";
    }

    auto degree = static_cast<int>(angle);
    auto minutes = (angle - degree) * static_cast<qreal>(60.0f);
    strdegree.sprintf("%02d",static_cast<int>(degree));
    strminutes.sprintf("%2.3f",minutes);
    output = strdegree + strminutes.remove(".") + hemisphere;

    //int seconds = (int)(angle - (float)degree - (float)minutes/60.f) * 60.f * 60.f); // ignore this line if you don't need seconds
    return output;
}

QString MainWindow::decimalToDDDMMMMMLon(double angle)
{
    QString output, strdegree, strminutes, hemisphere;

    if (angle < 0) {
        angle = -1 * angle;
        hemisphere = "W";
    } else {
        hemisphere = "E";
    }

    auto degree =static_cast<int>(angle);
    double minutes = (angle - degree) * static_cast<qreal>(60.f);
    strdegree.sprintf("%03d",static_cast<int>(degree));
    strminutes.sprintf("%2.3f",minutes);
    output = strdegree + strminutes.remove(".") + hemisphere;

    //int seconds = (int)(angle - (float)degree - (float)minutes/60.f) * 60.f * 60.f); // ignore this line if you don't need seconds
    return output;
}

void MainWindow::on_buttonStart_clicked()
{
    if (m_running)
    {
        varioBeep->stopBeep();
        m_posSource->stopUpdates();
        ui->buttonStart->setText("Start");
        m_running = false;
    }
    else
    {
        m_posSource->startUpdates();
        varioBeep->startBeep();
        ui->buttonStart->setText("Stop");
        m_running = true;
    }
}


void MainWindow::on_pushButton_exit_clicked()
{
   exitApp();
}

void MainWindow::exitApp()
{
    varioBeep->stopBeep();
    qApp->quit();
}
