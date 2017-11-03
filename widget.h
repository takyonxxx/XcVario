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
#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QGeoPositionInfoSource>
#include <QTableWidget>
#include <qsensor.h>
#include <QPressureReading>
#include <QDateTime>
#include <QTableWidget>
#include <QTimer>
#include <math.h>
#include <QFile>
#include <kalmanfilter.h>
#include "variobeep.h"
#include <QAndroidJniEnvironment>
#include <QAndroidJniObject>

namespace Ui {
    class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

private:
    void showEvent(QShowEvent *event);
    void fillTable(qreal dt,qreal pressure,qreal altitude,qreal vario);
    void fillVario(qreal vario);
    void createTables();
    void updateIGC();
    void createIgcHeader();
    QString decimalToDDDMMMMMLat(double angle);
    QString decimalToDDDMMMMMLon(double angle);

public slots:
    void positionUpdated(QGeoPositionInfo gpsPos);
    void setInterval(int msec);   
    void errorChanged(QGeoPositionInfoSource::Error err);
    void loadSensors();
    void sensor_changed();

    void on_buttonStart_clicked();   
    void on_pushButton_exit_clicked();

private:
    Ui::Widget *ui;
    VarioBeep *beep;

    QGeoPositionInfoSource *m_posSource;
    QGeoPositionInfo m_gpsPos;
    QGeoCoordinate m_coord;
    bool m_running;
    bool createIgcFile;

    QPressureSensor *m_sensor;
    QPressureReading *pressure_reading;
    QString text_presssure;
    QDateTime start;
    QDateTime end;
    QTableWidget *tableSensor;
    QTableWidget *tableGps;

    KalmanFilter *pressure_filter;
    KalmanFilter *altitude_filter;
    qreal dt;
    qreal pressure;
    qreal temperature;
    qreal baroaltitude;
    qreal altitude;
    qreal vario;
    qreal oldaltitude;   
    QFile * igcFile;
    QAndroidJniObject mediaDir;
};

#endif // WIDGET_H
