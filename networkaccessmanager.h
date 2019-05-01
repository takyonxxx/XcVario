#ifndef NETWORKACCESSMANAGER_H
#define NETWORKACCESSMANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QFile>

class NetworkAccessManager :public QObject
{
    Q_OBJECT

public:
    NetworkAccessManager(QUrl &url, QObject* parent);
    void sendRequest(const QFile &file);

private:
    QNetworkAccessManager * manager;
    QNetworkRequest request;

public slots:
    void replyFinished(QNetworkReply *reply);
};

#endif // NETWORKACCESSMANAGER_H
