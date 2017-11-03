/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "widget.h"
#include "ui_widget.h"
#include <QGeoPositionInfoSource>
#include <QDebug>


#define KF_VAR_ACCEL 0.0075 // Variance of pressure acceleration noise input.
#define KF_VAR_MEASUREMENT 0.05
#define sealevel 101325.0
#define DURATION_MS 1000
Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
    m_running = false;
    createIgcFile = false;

    mediaDir = QAndroidJniObject::callStaticObjectMethod("android/os/Environment", "getExternalStorageDirectory", "()Ljava/io/File;");
    QDir dir(mediaDir.toString() + "/XcVario");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    ui->label_vario->setStyleSheet("color: #cccccc; background-color: #001a1a;");
    ui->label_time->setStyleSheet("font-size: 22pt; color: white; background-color: #001a1a;");
    pressure = 101325.0;
    altitude = 0;
    oldaltitude = 0;
    createTables();

    beep = new VarioBeep(750.0,DURATION_MS * 1000.0);
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
        ui->label_time->setText(availableSources);
    }   
}

Widget::~Widget()
{
    if(igcFile->isOpen())
    {
        igcFile->close();
    }

    delete ui;
}

void Widget::createTables()
{
    tableSensor = new QTableWidget(this);
    QStringList m_SensorHeader;
    tableSensor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tableSensor->setRowCount(1);
    tableSensor->setColumnCount(4);
    m_SensorHeader<<"Refresh"<<"Pressure (Pa)"<<"Altitude (m)"<<"Vario (m/s)";
    tableSensor->setHorizontalHeaderLabels(m_SensorHeader);
    tableSensor->verticalHeader()->setVisible(false);
    tableSensor->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableSensor->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableSensor->setSelectionMode(QAbstractItemView::SingleSelection);
    tableSensor->setShowGrid(false);
    tableSensor->horizontalHeader()->setStyleSheet("QHeaderView { font-size: 14pt;color: #00cccc; background-color: #001a1a; }");
    tableSensor->setStyleSheet("QTableView {font-size: 16pt; color: white; background-color: #001a1a;}");

    tableSensor->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    //Height of the table
    int iHeight = 0;
    for (int i=0; i<tableSensor->rowCount(); i++)
    {
        iHeight += tableSensor->verticalHeader()->sectionSize(i);
    }
    iHeight += tableSensor->horizontalHeader()->height();
    tableSensor->setMaximumHeight(iHeight);

    tableSensor->verticalHeader()->setDefaultAlignment(Qt::AlignHCenter);

    tableGps = new QTableWidget(this);
    QStringList m_GpsHeader;
    tableGps->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tableGps->setRowCount(1);
    tableGps->setColumnCount(4);
    m_GpsHeader<<"Latitude"<<"Longitude"<<"Altitude (m)"<<"Speed (km/h)";
    tableGps->setHorizontalHeaderLabels(m_GpsHeader);
    tableGps->verticalHeader()->setVisible(false);
    tableGps->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableGps->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableGps->setSelectionMode(QAbstractItemView::SingleSelection);
    tableGps->setShowGrid(false);
    tableGps->horizontalHeader()->setStyleSheet("QHeaderView { font-size: 14pt;color: #00cccc; background-color: #001a1a; }");
    tableGps->setStyleSheet("QTableView {font-size: 16pt; color: white; background-color: #001a1a;}");

    tableGps->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    //Height of the table
    iHeight = 0;
    for (int i=0; i<tableGps->rowCount(); i++)
    {
        iHeight += tableGps->verticalHeader()->sectionSize(i);
    }
    iHeight += tableGps->horizontalHeader()->height();
    tableGps->setMaximumHeight(iHeight);

    tableGps->verticalHeader()->setDefaultAlignment(Qt::AlignHCenter);

    QVBoxLayout* layout = ui->verticalLayout;
    layout->addWidget(tableSensor);
    layout->addWidget(tableGps);
}

void Widget::loadSensors()
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

void Widget::showEvent(QShowEvent *event)
{
    // Once we're visible, load the sensors
    // (don't delay showing the UI while we load plugins, connect to backends, etc.)
    QTimer::singleShot(0, this, SLOT(loadSensors()));
    QWidget::showEvent(event);
}

void Widget::sensor_changed()
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

        fillTable(dt,pressure,altitude,vario);
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

void Widget::fillVario(qreal vario)
{
    ui->label_vario->setText(
                "<span style='font-size:110pt; font-weight:600;'>"
               + QString::number(vario, 'f', 1) +"</span>"
               + "<span style='font-size:36pt; font-weight:600; color:#00cccc;'> m/s</span>"
               );

}

void Widget::fillTable(qreal dt,qreal pressure,qreal altitude,qreal vario)
{
    QModelIndex index ;

    index = tableSensor->model()->index(0, 0);
    tableSensor->model()->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
    tableSensor->model()->setData(index, QString::number(dt, 'f', 2));

    index = tableSensor->model()->index(0, 1);
    tableSensor->model()->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
    tableSensor->model()->setData(index, QString::number(pressure, 'f', 1));

    index = tableSensor->model()->index(0, 2);
    tableSensor->model()->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
    tableSensor->model()->setData(index, QString::number(altitude, 'f', 1));

    index = tableSensor->model()->index(0, 3);
    tableSensor->model()->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
    tableSensor->model()->setData(index, QString::number(vario, 'f', 2));

    index = tableGps->model()->index(0, 0);
    tableGps->model()->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
    tableGps->model()->setData(index, QString::number(m_coord.latitude(), 'f', 4));

    index = tableGps->model()->index(0, 1);
    tableGps->model()->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
    tableGps->model()->setData(index, QString::number(m_coord.longitude(), 'f', 4));

    index = tableGps->model()->index(0, 2);
    tableGps->model()->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
    tableGps->model()->setData(index, QString::number(m_coord.altitude()));

    index = tableGps->model()->index(0, 3);
    tableGps->model()->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
    tableGps->model()->setData(index, QString::number(3.6 * m_gpsPos.attribute(QGeoPositionInfo::GroundSpeed)));

    /*if (m_gpsPos.hasAttribute(QGeoPositionInfo::Direction))
        QString::number(m_gpsPos.attribute(QGeoPositionInfo::Direction));*/

}

//////////////////////////////////

void Widget::positionUpdated(QGeoPositionInfo gpsPos)
{
    m_gpsPos = gpsPos;
    m_coord = gpsPos.coordinate();
    QDateTime timestamp = gpsPos.timestamp();
    QDateTime local = timestamp.toLocalTime();
    qreal horizontalAccuracy = gpsPos.attribute(QGeoPositionInfo::HorizontalAccuracy);
    QString dateTimeString = local.toString("hh : mm : ss");
    //ui->label_time->setText(dateTimeString + "\nAcc: " + QString::number(horizontalAccuracy, 'f', 1) + " m");
    ui->label_time->setText(
                "<span style='font-size:22pt; font-weight:600;color:#ff6600;'>"
               + dateTimeString + "</span>" + "<br />"
               + "<span style='font-size:22pt; font-weight:600; color:white;'>Acc: "
               + QString::number(horizontalAccuracy, 'f', 1) + " m</span>"
               );

    updateIGC();
}

void Widget::errorChanged(QGeoPositionInfoSource::Error err)
{
    qDebug() << (err == 3 ? QStringLiteral("No error") : QString::number(err));
}

void Widget::setInterval(int msec)
{
    m_posSource->setUpdateInterval(msec);
}

void Widget::on_buttonStart_clicked()
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


void Widget::on_pushButton_exit_clicked()
{
    beep->stopBeep();
    exit(0);
}

//gps
void Widget::updateIGC()
{
    if(!createIgcFile)
    {
        QString sdate = QDate::currentDate().toString("ddMMyy");
        QString stime = QTime::currentTime().toString("hhmmss");
        igcFile = new QFile(mediaDir.toString() + "/XcVario/" + sdate + "-" + stime +"_flightlog.igc");
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

void Widget::createIgcHeader()
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

QString Widget::decimalToDDDMMMMMLat(double angle)
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

QString Widget::decimalToDDDMMMMMLon(double angle)
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
