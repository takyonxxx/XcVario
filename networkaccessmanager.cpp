#include "networkaccessmanager.h"

NetworkAccessManager::NetworkAccessManager(QUrl &url, QObject* parent):
    QObject(parent)
{    
    manager = new QNetworkAccessManager(this);

    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    connect(manager, &QNetworkAccessManager::finished, this, &NetworkAccessManager::replyFinished);
}

void NetworkAccessManager::sendRequest(const QFile &file)
{
    QUrlQuery params;
    params.addQueryItem("user", "username");
    params.addQueryItem("pass", "password");

    manager->post(request, params.query().toUtf8());
}

void NetworkAccessManager::replyFinished(QNetworkReply *reply)
{
    if(reply->error())
    {
        qDebug()<<"error";
        qDebug()<<reply->errorString();
    }
    else
    {
        QString data = (QString) reply->readAll();
        qDebug() << data;
    }
}

