#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGeoPositionInfoSource>
#include <QTableWidget>
#include <QPressureReading>
#include <QDateTime>
#include <QTableWidget>
#include <QTimer>
#include <QtMath>
#include <QGeoPositionInfoSource>
#include <QFileDialog>
#include <QDesktopServices>
#include <QDebug>

#include <qsensor.h>
#include <kalmanfilter.h>
#include "variobeep.h"


#define KF_VAR_ACCEL 0.0075 // Variance of pressure acceleration noise input.
#define KF_VAR_MEASUREMENT 0.05
#define sealevel 101325.0
#define DURATION_MS 1000

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void showEvent(QShowEvent *event);   
    void fillVario(qreal vario);
    void createTables();
    void updateIGC();
    void createIgcHeader();
    QString decimalToDDDMMMMMLat(double angle);
    QString decimalToDDDMMMMMLon(double angle);
    QString path;

public slots:
    void positionUpdated(QGeoPositionInfo gpsPos);
    void setInterval(int msec);
    void errorChanged(QGeoPositionInfoSource::Error err);
    void loadSensors();
    void sensor_changed();

    void on_buttonStart_clicked();
    void on_pushButton_exit_clicked();
    void exitApp();

private:

    VarioBeep *varioBeep;

    QGeoPositionInfoSource *m_posSource;
    QGeoPositionInfo m_gpsPos;
    QGeoCoordinate m_coord;
    bool m_running;
    bool createIgcFile;

    QPressureSensor *m_sensor;
    QPressureReading *pressure_reading;
    QString text_presssure;
    QString text_igc_name;
    QDateTime start;
    QDateTime end;

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

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H

