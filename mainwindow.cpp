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

    ui->label_vario->setStyleSheet("font-size: 16pt; color: #cccccc; background-color: #001a1a;");
    ui->label_gps->setStyleSheet("font-size: 16pt; color: #cccccc; background-color: #001a1a;");

    beep = new VarioBeep(750.0, DURATION_MS * 1000.0);
    beep->setVolume(100);

    //Sensors
    (void)QSensor::sensorTypes();
    QSensor *sensor = new QSensor(QByteArray(), this);
    connect(sensor, SIGNAL(availableSensorsChanged()), this, SLOT(loadSensors()));

    //Gps
    qDebug() << "Available:" << QGeoPositionInfoSource::availableSources();
    m_posSource = QGeoPositionInfoSource::createDefaultSource(this);
    if (!m_posSource)
        qFatal("No Position Source created!");
    else
    {
        connect(m_posSource, SIGNAL(positionUpdated(QGeoPositionInfo)),
                this, SLOT(positionUpdated(QGeoPositionInfo)));

        connect(m_posSource, SIGNAL(error(QGeoPositionInfoSource::Error)),
                this, SLOT(errorChanged(QGeoPositionInfoSource::Error)));

        setInterval(0);
        QString availableSources;
        availableSources.append("Available gps sources\n");
        QStringList posSourcesList = QGeoPositionInfoSource::availableSources();
        qDebug() << "Position sources count: " << posSourcesList.count();
        foreach (const QString &src, posSourcesList) {
            qDebug() << "pos source in list: " << src;
            availableSources.append(src);
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
    status.append("Searching sensors.");
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
                    m_sensor = 0;
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

    if(pressure_reading != 0)
    {
        pressure = pressure_reading->pressure();
        temperature = pressure_reading->temperature();
        //(pressure, KF_VAR_MEASUREMENT, millisecondsDiff);

        pressure_filter->Update(pressure,KF_VAR_MEASUREMENT,dt);
        pressure = pressure_filter->GetXAbs();

        baroaltitude = 44330.0 * (1.0 - powf(pressure /sealevel, 0.19));
        altitude_filter->Update(baroaltitude,KF_VAR_MEASUREMENT,dt);
        altitude = altitude_filter->GetXAbs();
        vario = altitude_filter->GetXVel();
        beep->SetVario(vario,dt);

        fillGps(dt,pressure,altitude,vario);
        fillVario(vario);
    }
    else
    {
        text_presssure = "\nSensor: UNAVAILABLE";
        //ui->textBrowser_info->append(text_presssure);
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

void MainWindow::fillGps(qreal dt,qreal pressure,qreal altitude,qreal vario)
{
    if (m_gpsPos.hasAttribute(QGeoPositionInfo::Direction))
    {
        ui->label_vario->setText(
                    "<span style='font-size:110pt; font-weight:600;'>"
                    + QString::number(m_coord.altitude())
                    + " / " +  QString::number(3.6 * m_gpsPos.attribute(QGeoPositionInfo::GroundSpeed))
                    + "</span>"
                    + "<span style='font-size:36pt; font-weight:600; color:#00cccc;'> m/s</span>"
                    );

    }

    /*QString::number(pressure, 'f', 1)
    QString::number(altitude, 'f', 1)
    QString::number(vario, 'f', 2)
    QString::number(m_gpsPos.attribute(QGeoPositionInfo::Direction));
    QString::number(m_coord.altitude())
    QString::number(3.6 * m_gpsPos.attribute(QGeoPositionInfo::GroundSpeed))*/
}

//////////////////////////////////

void MainWindow::positionUpdated(QGeoPositionInfo gpsPos)
{
    m_gpsPos = gpsPos;
    m_coord = gpsPos.coordinate();
    QDateTime timestamp = gpsPos.timestamp();
    QDateTime local = timestamp.toLocalTime();
    qreal horizontalAccuracy = gpsPos.attribute(QGeoPositionInfo::HorizontalAccuracy);
    QString dateTimeString = local.toString("hh : mm : ss");
    //ui->label_time->setText(dateTimeString + "\nAcc: " + QString::number(horizontalAccuracy, 'f', 1) + " m");
    ui->label_gps->setText(
                "<span style='font-size:22pt; font-weight:600;color:#ff6600;'>"
                + dateTimeString + "</span>" + "<br />"
                + "<span style='font-size:22pt; font-weight:600; color:white;'>Acc: "
                + QString::number(horizontalAccuracy, 'f', 1) + " m</span>"
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

void MainWindow::on_buttonStart_clicked()
{
    if (m_running) {
        beep->stopBeep();
        m_posSource->stopUpdates();
        m_running = false;
        ui->buttonStart->setText("Start");
    } else {
        m_posSource->startUpdates();
        m_running = true;
        beep->startBeep();
        ui->buttonStart->setText("Stop");
    }
}


void MainWindow::on_pushButton_exit_clicked()
{
    beep->stopBeep();
    exit(0);
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
    str.sprintf("%05d",(int)m_coord.altitude());
    record.append(str);
    str.sprintf("%05d",(int)altitude);
    record.append(str);
    out << record << endl;

    igcFile->close();
}

void MainWindow::createIgcHeader()
{
    igcFile->open(QIODevice::Append | QIODevice::Text);

    if(!igcFile->isOpen()){
        qDebug() << "- Error, unable to open" << "outputFilename" << "for output";
    }

    QFileInfo fileInfo(igcFile->fileName());
    qDebug() << "igc file path: " << fileInfo.absoluteFilePath();

    QTextStream out(igcFile);

    QDateTime timestamp = m_gpsPos.timestamp();
    QString dateString = timestamp.toString("ddMMyy");

    QString header  = "AXGD000 XcVario v1.0\n";
    header.append("HFDTE" + dateString + "\n");
    header.append("HODTM100GPSDATUM: WGS-84\n");
    header.append("HFFTYFRTYPE: XcVario by Türkay Biliyor");
    out << header << endl;
    igcFile->close();
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

    int degree = (int) angle;
    double minutes = (angle - (float)degree) * 60.f;
    strdegree.sprintf("%02d",(int)degree);
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

    int degree = (int) angle;
    double minutes = (angle - (float)degree) * 60.f;
    strdegree.sprintf("%03d",(int)degree);
    strminutes.sprintf("%2.3f",minutes);
    output = strdegree + strminutes.remove(".") + hemisphere;

    //int seconds = (int)(angle - (float)degree - (float)minutes/60.f) * 60.f * 60.f); // ignore this line if you don't need seconds
    return output;
}
