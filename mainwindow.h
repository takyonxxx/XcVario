#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QSettings>
#include <QMessageBox>

 #ifdef Q_OS_WIN
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

#include <QPressureReading>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QGeoSatelliteInfoSource>
#include <QGeoSatelliteInfo>
#include <QNmeaPositionInfoSource>
#include <QStandardPaths>
#include <QDateTime>
#include <QtMath>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QFileDialog>
#include <QDesktopServices>

#include <QDebug>

#include <networkaccessmanager.h>
#include <qsensor.h>
#include <kalmanfilter.h>
#include "variobeep.h"
#include "logindialog.h"

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

    static void SetTextToLabel(QLabel *label, QString text)
    {
        QFontMetrics metrix(label->font());
        int width = label->width() - 2;
        QString clippedText = metrix.elidedText(text, Qt::ElideRight, width);
        label->setText(clippedText);
    }

private:
    void showEvent(QShowEvent *event);
    bool startNmeaSource();
    bool startGpsSource();
    void startSensors();
    void fillVario();
    void fillAltitude();
    void createTables();
    void updateIGC();
    void createIgcHeader();
    void loadSettings();
    void saveSettings();
    void openLoginDialog();

    QString decimalToDDDMMMMMLat(double angle);
    QString decimalToDDDMMMMMLon(double angle);   

public slots:
    void positionUpdated(QGeoPositionInfo gpsPos);
    void setInterval(int msec);
    void errorChanged(QGeoPositionInfoSource::Error err);
    void loadSensors();
    void sensor_changed();
    void satellitesInViewUpdated(const QList<QGeoSatelliteInfo> &infos);
    void satellitesInUseUpdated(const QList<QGeoSatelliteInfo> &infos);
    void updateTimeout(void);
    void slotAcceptUserLogin(QString&,QString&);
    void invalidUser();
    void responseResult(const QString &result);
    void on_gpsLabel_linkActivated(const QString & link);

    void on_buttonStart_clicked();
    void on_pushButton_exit_clicked();
    void exitApp();
    void on_buttonFile_clicked();

private:

    VarioBeep *varioBeep;
    NetworkAccessManager *networkmanager;

    QGeoPositionInfoSource *m_posSource;
    QNmeaPositionInfoSource *m_nmeaSource;
    QGeoPositionInfo m_gpsPos;
    QGeoCoordinate m_coord;
    QGeoCoordinate m_startCoord;

    bool m_sensorPressureValid;
    bool m_start;
    bool m_running;
    bool createIgcFile;

    QPressureSensor *m_sensor;
    QPressureReading *pressure_reading;

    QString m_SettingsFile;
    QString igcFileName;
    QString user;
    QString pass;
    QString path;
    QString text_presssure;
    QString text_igc_name;

    QDateTime start;
    QDateTime end;

    KalmanFilter *pressure_filter;
    KalmanFilter *altitude_filter;

    qreal distance;
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

