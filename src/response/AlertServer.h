#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>

class AlertServer : public QTcpServer {
    Q_OBJECT

public:
    explicit AlertServer(QObject *parent = nullptr);
    void startServer(quint16 port = 5555);

    signals:
        void alertReceived(QJsonObject alert);

protected:
    void incomingConnection(qintptr socketDescriptor) override;
};
