#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPressureReading>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QGeoSatelliteInfoSource>
#include <QGeoSatelliteInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QtMath>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QFileDialog>
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

    bool IsNan( float value )
    {
        return ((*(uint*)&value) & 0x7fffffff) > 0x7f800000;
    }

    void clearDir( const QString path )
    {
        QDir dir( path );

        dir.setFilter( QDir::NoDotAndDotDot | QDir::Files );
        foreach( QString dirItem, dir.entryList() )
            dir.remove( dirItem );

        dir.setFilter( QDir::NoDotAndDotDot | QDir::Dirs );
        foreach( QString dirItem, dir.entryList() )
        {
            QDir subDir( dir.absoluteFilePath( dirItem ) );
            subDir.removeRecursively();
        }
    }

private:
    void showEvent(QShowEvent *event);
    void fillVario();
    void fillAltitude();
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
    void satellitesInViewUpdated(const QList<QGeoSatelliteInfo> &infos);
    void satellitesInUseUpdated(const QList<QGeoSatelliteInfo> &infos);

    void on_buttonStart_clicked();
    void on_pushButton_exit_clicked();
    void exitApp();

private slots:
    void on_buttonFile_clicked();

private:

    VarioBeep *varioBeep;

    QGeoPositionInfoSource *m_posSource;
    QGeoPositionInfo m_gpsPos;
    QGeoCoordinate m_coord;
    QGeoCoordinate m_startCoord;
    bool m_start;
    bool m_sensorPressureValid;
    qreal distance;

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
    qreal speed;
    qreal oldaltitude;
    QFile * igcFile;

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H

