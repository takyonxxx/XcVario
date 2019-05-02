#include "networkaccessmanager.h"

NetworkAccessManager::NetworkAccessManager(QUrl &url, QObject* parent):
    QObject(parent)
{    
    manager = new QNetworkAccessManager(this);

    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    connect(manager, &QNetworkAccessManager::finished, this, &NetworkAccessManager::replyFinished);
}

void NetworkAccessManager::sendRequest(const QString &user, const QString &pass, QFile &file)
{
    QUrlQuery params;
    QFileInfo fileInfo(file);

    if(!file.open(QIODevice::ReadOnly))
    {
        qDebug() << file.errorString();
        return;
    }

    QTextStream  in(&file);

    params.addQueryItem("user", user);
    params.addQueryItem("pass", pass);
    params.addQueryItem("igcfn", fileInfo.fileName().remove(".igc"));
    params.addQueryItem("Klasse", "3");
    params.addQueryItem("IGCigcIGC", in.readAll());

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
        if(data.contains("Invalid user data"))
            emit invalidUser();
        qDebug() << data;
    }
}

