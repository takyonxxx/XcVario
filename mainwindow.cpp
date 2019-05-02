#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    varioBeep(nullptr),
    networkmanager(nullptr),
    m_posSource(nullptr),
    m_nmeaSource(nullptr),
    m_sensorPressureValid(false),
    m_start(false),
    m_running(false),
    createIgcFile(false),
    distance(0),
    pressure (101325.0),
    altitude (0),
    vario (0),
    speed (0),
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

    ui->label_vario->setStyleSheet("font-size: 16pt; color: #cccccc; background-color: #001a1a;");
    ui->label_gps->setStyleSheet("font-size: 16pt; color: #cccccc; background-color: #001a1a;");
    ui->label_altitude->setStyleSheet("font-size: 16pt; color: #cccccc; background-color: #001a1a;");
    ui->buttonStart->setEnabled(false);

    startSensors();

    QString status;
    if(!startGpsSource())
    {
        status.append("<span style='font-size:18pt; font-weight:600;color:#00cccc;'>No core gps source found!</span><br />");
        ui->label_gps->setText(status);

        if(!startNmeaSource())
        {
            status.append("<span style='font-size:18pt; font-weight:600;color:#00cccc;'>No nmea source found!</span>");
            ui->label_gps->setText(status);
        }
        else
        {
            ui->buttonStart->setEnabled(true);
        }
    }
    else
    {
        ui->buttonStart->setEnabled(true);
    }

    //QUrl url = QUrl("http://xc.dhv.de/xc/modules/leonardo/flight_submit.php");
    //QUrl url = QUrl("http://www.paraglidingforum.com/modules/leonardo/flight_submit.php");
    QUrl url = QUrl("http://www.ypforum.com/modules/leonardo/flight_submit.php");

    networkmanager = new NetworkAccessManager(url, this);
    connect(networkmanager, &NetworkAccessManager::invalidUser, this, &MainWindow::invalidUser);
}

MainWindow::~MainWindow()
{
    if(igcFile->isOpen())
    {
        igcFile->close();
    }
    delete ui;
}

void MainWindow::startSensors()
{
    (void)QSensor::sensorTypes();
    QSensor *sensor = new QSensor(QByteArray(), this);
    connect(sensor, SIGNAL(availableSensorsChanged()), this, SLOT(loadSensors()));
}

bool MainWindow::startNmeaSource()
{
    auto serial = new QSerialPort(this);
    QList<QSerialPortInfo> com_ports = QSerialPortInfo::availablePorts();

    if (com_ports.isEmpty()) {
        qWarning("serialnmea: No serial ports found");
        return false;
    }

    QSet<int> supportedDevices;
    supportedDevices << 0x67b;  // GlobalSat (BU-353S4 and probably others)
    supportedDevices << 0xe8d;  // Qstarz MTK II
    supportedDevices << 0x1546; // u-blox GNNS

    QString portName;
    QString vendorId;
    bool deviceFound = false;
    foreach (const QSerialPortInfo& port, com_ports) {
        if (port.hasVendorIdentifier() && supportedDevices.contains(port.vendorIdentifier())) {
            portName = port.portName();
            vendorId = (port.hasVendorIdentifier()
                        ? QByteArray::number(port.vendorIdentifier(), 16) : "no vendor id");
            serial->setPortName(portName);
            deviceFound = true;
            break;
        }
    }

    if(!deviceFound)
        return false;

    if(!serial->setBaudRate(QSerialPort::Baud4800))
        qDebug() << serial->errorString();
    if(!serial->setDataBits(QSerialPort::Data7))
        qDebug() << serial->errorString();
    if(!serial->setParity(QSerialPort::EvenParity))
        qDebug() << serial->errorString();
    if(!serial->setFlowControl(QSerialPort::HardwareControl))
        qDebug() << serial->errorString();
    if(!serial->setStopBits(QSerialPort::OneStop))
        qDebug() << serial->errorString();

    if (!serial->open(QIODevice::ReadOnly))
    {
        qWarning("Failed to open %s", qPrintable(serial->portName()));
        serial->reset();
        return false;
    }

    m_nmeaSource = new QNmeaPositionInfoSource(QNmeaPositionInfoSource::RealTimeMode, this);
    if (m_nmeaSource == nullptr)
    {
        return false;
    }

    m_nmeaSource->setDevice(serial);
    m_nmeaSource->setUpdateInterval(0);

    QIODevice* dev = m_nmeaSource->device();
    if(dev)
    {
        connect(m_nmeaSource, &QGeoPositionInfoSource::positionUpdated, this, &MainWindow::positionUpdated);
        connect(m_nmeaSource, &QGeoPositionInfoSource::updateTimeout, this, &MainWindow::updateTimeout);
        connect(m_nmeaSource, SIGNAL(error(QGeoPositionInfoSource::Error)), this, SLOT(errorChanged(QGeoPositionInfoSource::Error)));

        QString status;
        status.append("<span style='font-size:18pt; font-weight:600;color:#00cccc;'>Gps nmea device found</span><br />");
        status.append("Port: " +  serial->portName() + " VendorId: " + vendorId);
        ui->label_gps->setText(status);

        /*while(serial->isOpen())
        {
            if(!serial->waitForReadyRead(-1)) //block until new data arrives
                qDebug() << "error: " << serial->errorString();
            else{
                QByteArray datas = serial->readAll();
                qDebug() << datas << "\n";
            }
        }*/
    }
    else
    {
        qDebug() << "failed to get device";
        return false;
    }

    return true;
}

bool MainWindow::startGpsSource()
{
    m_posSource = QGeoPositionInfoSource::createDefaultSource(this);
    if (m_posSource == nullptr)
    {
        return false;
    }

    if(!m_posSource->supportedPositioningMethods())
    {
        m_posSource = nullptr;
        return false;
    }

    m_posSource->setPreferredPositioningMethods(QGeoPositionInfoSource::AllPositioningMethods);
    m_posSource->setUpdateInterval(0);
    connect(m_posSource, &QGeoPositionInfoSource::positionUpdated, this, &MainWindow::positionUpdated);
    connect(m_posSource, &QGeoPositionInfoSource::updateTimeout, this, &MainWindow::updateTimeout);
    connect(m_posSource, SIGNAL(error(QGeoPositionInfoSource::Error)), this, SLOT(errorChanged(QGeoPositionInfoSource::Error)));

    QString status;
    status.append("<span style='font-size:18pt; font-weight:600;color:#00cccc;'>Active Gps Source is:</span><br />");
    status.append( m_posSource->sourceName() + "<br />");
    ui->label_gps->setText(status);

    /*QGeoSatelliteInfoSource *satelliteSource = QGeoSatelliteInfoSource::createDefaultSource(this);
    if(satelliteSource)
    {
        satelliteSource->startUpdates();
        connect(satelliteSource, &QGeoSatelliteInfoSource::satellitesInViewUpdated,
                this, &MainWindow::satellitesInViewUpdated);
    }*/

    return true;
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

                varioBeep = new VarioBeep(750.0, static_cast<int>(DURATION_MS * 1000), this);
                varioBeep->setVolume(100);

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
        if(varioBeep)
            varioBeep->SetVario(vario);

        fillVario();
    }
    else
    {
        text_presssure = "\nSensor: UNAVAILABLE";
    }
    oldaltitude = altitude;
    start = end;
}

void MainWindow::fillVario()
{
    ui->label_vario->setText(
                "<span style='font-size:110pt; font-weight:600; color:#FFDD33;'>"
                + QString::number(vario, 'f', 1) +"</span>"
                + "<span style='font-size:36pt; font-weight:600; color:#00cccc;'> m/s</span>"
                );

}

void MainWindow::fillAltitude()
{
    ui->label_altitude->setText(
                "<span style='font-size:70pt; font-weight:600; color:#F78181;'>"
                + QString::number(speed, 'f', 1) +"</span>"
                + "<span style='font-size:36pt; font-weight:600; color:#00cccc;'> km/h</span><br />"
                + "<span style='font-size:70pt; font-weight:600; color:#F78181;'>"
                + QString::number(altitude, 'f', 1) +"</span>"
                + "<span style='font-size:36pt; font-weight:600; color:#00cccc;'> m</span>"
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
    if (!gpsPos.isValid() || !gpsPos.coordinate().isValid())
        return;

    m_gpsPos = gpsPos;
    m_coord = gpsPos.coordinate();

    if(!m_start)
    {
        m_startCoord = m_coord;
        m_start = true;
    }

    if(m_start)
    {
        distance = m_coord.distanceTo(m_startCoord) / 1000;
    }

    auto m_latitude = m_coord.latitude();
    auto m_longitude = m_coord.longitude();
    //if (m_coord.type() == QGeoCoordinate::Coordinate3D)
    auto m_altitude = m_coord.altitude();

    auto m_direction = m_gpsPos.attribute(QGeoPositionInfo::Direction);
    if(IsNan(static_cast<float>(m_direction))) m_direction = 0;

    auto m_groundSpeed = 3.6 * m_gpsPos.attribute(QGeoPositionInfo::GroundSpeed);
    if(IsNan(static_cast<float>(m_groundSpeed))) m_groundSpeed = 0;
    speed = m_groundSpeed;

    auto m_verticalSpeed = m_gpsPos.attribute(QGeoPositionInfo::VerticalSpeed);
    if(IsNan(static_cast<float>(m_verticalSpeed))) m_verticalSpeed = 0;

    auto m_horizontalAccuracy = m_gpsPos.attribute(QGeoPositionInfo::HorizontalAccuracy);
    if(IsNan(static_cast<float>(m_horizontalAccuracy))) m_horizontalAccuracy = 0;

    auto m_verticalAccuracy = m_gpsPos.attribute(QGeoPositionInfo::VerticalAccuracy);
    if(IsNan(static_cast<float>(m_verticalAccuracy))) m_verticalAccuracy = 0;

    auto m_magneticVariation = m_gpsPos.attribute(QGeoPositionInfo::MagneticVariation);
    if(IsNan(static_cast<float>(m_magneticVariation))) m_magneticVariation = 0;

    auto timestamp = gpsPos.timestamp();
    auto local = timestamp.toLocalTime();
    auto dateTimeString = local.toString("hh : mm : ss");
    text_igc_name = "VarioLog_" + local.toString("dd_MM_yyyy__hh_mm_ss") + ".igc";

    ui->label_gps->setText(
                "<br /><span style='font-size:32pt; font-weight:600;color:#00cccc;'>"
                + dateTimeString + "</span>" + "<br />"
                + "<span style='font-size:18pt; font-weight:600; color:#F2EDED;'>Altitude: "
                + QString::number(m_altitude, 'f', 1) + " m</span>" + "<br />"
                + "<span style='font-size:18pt; font-weight:600; color:#F2EDED;'>Heading: "
                + QString::number(m_direction, 'f', 0) + QObject::tr(" °") + "</span>" + "<br />"
                + "<span style='font-size:18pt; font-weight:600; color:#F2EDED;'>Distance: "
                + QString::number(distance, 'f', 1) + " km</span>" + "<br />"
                + "<span style='font-size:18pt; font-weight:600; color:#FFC0C0;'>"
                + QString("Latitude: %1").arg(m_latitude) + "</span>" + "<br />"
                + "<span style='font-size:18pt; font-weight:600; color:#FFC0C0;'>"
                + QString("Longitude: %1").arg(m_longitude) + "</span>" + "<br />"
                );

    if(!m_sensorPressureValid)
    {
        altitude = m_altitude;
        vario = m_verticalSpeed;
        fillVario();
    }

    fillAltitude();
    updateIGC();
}

void MainWindow::updateTimeout(void)
{
    qDebug() << "updateTimeout";
}

void MainWindow::errorChanged(QGeoPositionInfoSource::Error err)
{
    qDebug() << "errorChanged";
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
        QDir dir;
        if (!dir.exists(path))
            dir.mkpath(path);

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
        if(varioBeep)
            varioBeep->stopBeep();

        if(m_posSource != nullptr)
            m_posSource->stopUpdates();
        else if(m_nmeaSource != nullptr)
            m_nmeaSource->stopUpdates();

        ui->buttonStart->setText("Start");
        vario = 0;
        altitude = 0;
        speed = 0;
        m_running = false;
    }
    else
    {
        if(m_posSource != nullptr)
            m_posSource->startUpdates();
        else if(m_nmeaSource != nullptr)
            m_nmeaSource->startUpdates();

        if(varioBeep)
        {
            varioBeep->SetVario(0.0);
            varioBeep->startBeep();
        }

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
    if(varioBeep)
        varioBeep->stopBeep();
    qApp->quit();
}

void MainWindow::loadSettings()
{
    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    user = settings.value("user", "").toString();
    pass = settings.value("pass", "").toString();
}

void MainWindow::saveSettings()
{
    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    settings.setValue("user", user);
    settings.setValue("pass", pass);
}

void MainWindow::openLoginDialog()
{
    LoginDialog* loginDialog = new LoginDialog( this );
    connect( loginDialog,
             &LoginDialog::acceptLogin,
             this,
             &MainWindow::slotAcceptUserLogin);
    loginDialog->exec();
}

void MainWindow::invalidUser()
{
    openLoginDialog();
}

void MainWindow::slotAcceptUserLogin(QString &user, QString &pass)
{
    this->user = user;
    this->pass = pass;
    saveSettings();

    if(user.isEmpty() || pass.isEmpty())
        return;

    auto fileName = QFileDialog::getOpenFileName(this, tr("Open Igc"), path, tr("Igc Files (*.igc)"));
    QFile igcFile(fileName);
    if(networkmanager)
        networkmanager->sendRequest(user, pass, igcFile);
}

void MainWindow::on_buttonFile_clicked()
{
    //clearDir(path);
    m_SettingsFile = QCoreApplication::applicationDirPath() + "/settings.ini";

    if (QFile(m_SettingsFile).exists())
    {
        loadSettings();
    }

    if(user.isEmpty() || pass.isEmpty())
    {
        openLoginDialog();
        return;
    }

    auto fileName = QFileDialog::getOpenFileName(this, tr("Open Igc"), path, tr("Igc Files (*.igc)"));
    QFile igcFile(fileName);
    if(networkmanager)
        networkmanager->sendRequest(user, pass, igcFile);
}
