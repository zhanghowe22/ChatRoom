#include "client.h"

#include <QDataStream>
#include <QFile>
#include <QFileInfo>

Client::Client(QObject* parent) : QObject(parent), m_socket(new QTcpSocket(this)) {
    connect(m_socket, &QTcpSocket::readyRead, this, &Client::readyRead);
}

void Client::connectToServer(const QString& host, quint16 port) { m_socket->connectToHost(host, port); }

void Client::sendMessage(const QString& message) {
    QByteArray data = message.toUtf8();
    m_socket->write(data);
}

void Client::sendFile(const QString& filePath) {
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray fileData = file.readAll();
        QDataStream out(m_socket);
        out << QString("FILE_TRANSFER:");
        out << QFileInfo(file).fileName();
        out << fileData;
    }
}

QString Client::readMessage() {
    QByteArray data = m_socket->readAll();
    return QString::fromUtf8(data);
}
