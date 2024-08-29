#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>

class Client : public QObject {
    Q_OBJECT

  public:
    Client(QObject* parent = nullptr);
    void connectToServer(const QString& host, quint16 port);
    void sendMessage(const QString& message);
    void sendFile(const QString& filePath);
    QString readMessage();

  signals:
    void readyRead();

  private:
    QTcpSocket* m_socket;
};

#endif  // CLIENT_H
