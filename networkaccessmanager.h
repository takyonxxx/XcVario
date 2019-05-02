#ifndef NETWORKACCESSMANAGER_H
#define NETWORKACCESSMANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QFile>
#include <QFileInfo>

class NetworkAccessManager :public QObject
{
    Q_OBJECT

public:
    NetworkAccessManager(QUrl &url, QObject* parent);
    void sendRequest(const QString &user, const QString &pass, QFile &file);

private:
    QNetworkAccessManager * manager;
    QNetworkRequest request;

public slots:
    void replyFinished(QNetworkReply *reply);
signals:
    void invalidUser();
    void responseResult(const QString &result);
};

#endif // NETWORKACCESSMANAGER_H
