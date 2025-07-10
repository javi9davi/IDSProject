#include "AlertServer.h"

void AlertServer::startServer(quint16 port) {
    if (!listen(QHostAddress::Any, port)) {
        qCritical() << "Failed to start server:" << errorString();
    } else {
        qDebug() << "Alert server listening on port" << port;
    }
}

void AlertServer::incomingConnection(qintptr socketDescriptor) {
    auto *socket = new QTcpSocket(this);
    socket->setSocketDescriptor(socketDescriptor);

    connect(socket, &QTcpSocket::readyRead, this, [=]() {
        QByteArray data = socket->readAll();
        auto doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) emit alertReceived(doc.object());
    });

    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}
