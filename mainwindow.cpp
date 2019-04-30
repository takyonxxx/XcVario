#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_sensorPressureValid(false),
    distance(0),
    m_running(false),
    createIgcFile(false),
    pressure (101325.0),
    altitude (0),
    oldaltitude(0),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("XcVario");


#ifdef Q_OS_ANDROID
    path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QString("/VarioLog/");
#endif

#ifdef Q_OS_WIN
    path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QString("/VarioLog/");
#endif

#ifdef Q_OS_IOS
    path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QString("/VarioLog/");
#endif

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
    {
        ui->label_gps->setText("No Gps Position Source created!");
    }
    else
    {

        m_posSource->setPreferredPositioningMethods(QGeoPositionInfoSource::AllPositioningMethods);

        connect(m_posSource, &QGeoPositionInfoSource::positionUpdated, this, &MainWindow::positionUpdated);

        connect(m_posSource, SIGNAL(error(QGeoPositionInfoSource::Error)),
                this, SLOT(errorChanged(QGeoPositionInfoSource::Error)));

        m_posSource->setUpdateInterval(0);

        QString status;
        status.append("<span style='font-size:18pt; font-weight:600;color:#00cccc;'>Available gps sources:</span><br />");
        QStringList posSourcesList = QGeoPositionInfoSource::availableSources();
        int count = 1;

        foreach (const QString &src, posSourcesList) {
            status.append(QString::number(count) + " - " + src + "<br />");
        }
        ui->label_gps->setText(status);

        QGeoSatelliteInfoSource *satelliteSource = QGeoSatelliteInfoSource::createDefaultSource(this);
        if(satelliteSource)
        {
            QStringList sourcesList = QGeoSatelliteInfoSource::availableSources();
            qDebug() << "Satellites sources count: " << sourcesList.count();
            foreach (const QString &src, sourcesList) {
                qDebug() << "source in list: " << src;
            }

            satelliteSource->startUpdates();
            connect(satelliteSource, &QGeoSatelliteInfoSource::satellitesInViewUpdated,
                    this, &MainWindow::satellitesInViewUpdated);
        }
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
    status.append("<span style='font-size:18pt; font-weight:600;color:#00cccc;'>Available sensors:</span><br />");

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
            status.append(QString::number(count) + " - " + sensor_type + "<br />");
            if(sensor_type.contains("Pressure"))
            {
                m_sensorPressureValid = true;
                m_sensor = new QPressureSensor(this);
                connect(m_sensor, SIGNAL(readingChanged()), this, SLOT(sensor_changed()));
                m_sensor->setIdentifier(identifier);
                if (!m_sensor->connectToBackend()) {
                    status.append("Can't connect to Backend sensor: " + sensor_type + "<br />");
                    delete m_sensor;
                    m_sensor = nullptr;
                    return;
                }
                found = true;
                if(m_sensor->start())
                {
                    status.append(sensor_type + " started succesfully.<br />");
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
        varioBeep->SetVario(vario);

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

static QString satellitesToString(const QList<QGeoSatelliteInfo> &satellites)
{
    QString text;

    foreach (const QGeoSatelliteInfo &info, satellites) {
        text += QObject::tr("PRN: %1\nAzimuth: %2\nElevation: %3\nSignal: %4\n")
                .arg(info.satelliteIdentifier())
                .arg(info.attribute(QGeoSatelliteInfo::Azimuth))
                .arg(info.attribute(QGeoSatelliteInfo::Elevation))
                .arg(info.signalStrength());
    }

    return text;
}

void MainWindow::satellitesInUseUpdated(const QList<QGeoSatelliteInfo> &satellites)
{
    if(satellites.size() == 0)
        return;

    qDebug() << tr("satellitesInUseUpdated received");

    QString status;
    status.append("<span style='font-size:18pt; font-weight:600;color:#00cccc;'>Satellites In Use:</span><br />");
    status.append(satellitesToString(satellites));

    ui->label_gps->setText(status);
}

void MainWindow::satellitesInViewUpdated(const QList<QGeoSatelliteInfo> &satellites)
{
    if(satellites.size() == 0)
        return;

    qDebug() << tr("satellitesInViewUpdated received");

    QString status;
    status.append("<span style='font-size:18pt; font-weight:600;color:#00cccc;'>Satellites In View:</span><br />");
    status.append(satellitesToString(satellites));

    ui->label_gps->setText(status);
}

void MainWindow::positionUpdated(QGeoPositionInfo gpsPos)
{
    if(!gpsPos.isValid())
        return;

    m_gpsPos = gpsPos;
    m_coord = gpsPos.coordinate();

    if(!m_start)
    {
        m_startCoord = gpsPos.coordinate();
        m_start = true;
    }

    if(m_start)
    {
        distance = m_coord.distanceTo(m_startCoord);
    }

    auto m_latitude = m_gpsPos.coordinate().latitude();
    auto m_longitude = m_gpsPos.coordinate().longitude();
    auto m_altitude = m_gpsPos.coordinate().altitude();

    auto m_direction = m_gpsPos.attribute(QGeoPositionInfo::Direction);
    auto m_groundSpeed = m_gpsPos.attribute(QGeoPositionInfo::GroundSpeed);
    auto m_verticalSpeed = m_gpsPos.attribute(QGeoPositionInfo::VerticalSpeed);
    auto m_horizontalAccuracy = m_gpsPos.attribute(QGeoPositionInfo::HorizontalAccuracy);
    auto m_verticalAccuracy = m_gpsPos.attribute(QGeoPositionInfo::VerticalAccuracy);
    auto m_magneticVariation = m_gpsPos.attribute(QGeoPositionInfo::MagneticVariation);

    auto timestamp = gpsPos.timestamp();
    auto local = timestamp.toLocalTime();
    auto dateTimeString = local.toString("hh : mm : ss");
    text_igc_name = "VarioLog_" + local.toString("dd_MM_yyyy__hh_mm_ss") + ".igc";

    ui->label_gps->setText(
                "<span style='font-size:22pt; font-weight:600;color:#00cccc;'>"
                + dateTimeString + "</span>" + "<br />"
                + "<span style='font-size:22pt; font-weight:600; color:white;'>Altitude: "
                + QString::number(m_altitude, 'f', 1) + " m</span>" + "<br />"
                + "<span style='font-size:22pt; font-weight:600; color:white;'>Speed: "
                + QString::number(m_groundSpeed, 'f', 0) + " km/h</span>" + "<br />"
                + "<span style='font-size:22pt; font-weight:600; color:white;'>Heading: "
                + QString::number(m_direction, 'f', 0) + QObject::tr(" °") + "</span>" + "<br />"
                + "<span style='font-size:22pt; font-weight:600; color:white;'>Distance: "
                + QString::number(distance / 1000, 'f', 1) + " km</span>" + "<br />"
                + "<span style='font-size:18pt; font-weight:600; color:#ff6600;'>"
                + QString("Latitude: %1").arg(m_latitude) + "</span>" + "<br />"
                + "<span style='font-size:18pt; font-weight:600; color:#ff6600;'>"
                + QString("Longitude: %1").arg(m_longitude) + "</span>"
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
        QString status;
        status.append("<span style='font-size:18pt; font-weight:600;color:#00cccc;'>Available gps sources:</span><br />");
        QStringList posSourcesList = QGeoPositionInfoSource::availableSources();
        int count = 1;

        foreach (const QString &src, posSourcesList) {
            status.append(QString::number(count) + " - " + src + "<br />");
        }
        ui->label_gps->setText(status);
        m_running = false;
    }
    else
    {
        m_posSource->startUpdates();
        varioBeep->SetVario(0.0);
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

void MainWindow::on_buttonFile_clicked()
{
    auto fileName = QFileDialog::getOpenFileName(this, tr("Open Igc"), path, tr("Igc Files (*.igc)"));
    QFileInfo info(fileName);
    QFile igcFile(fileName);
}
