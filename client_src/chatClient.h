#ifndef CLIENT_H
#define CLIENT_H

#include <QFile>
#include <QTcpSocket>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class Client;
}
QT_END_NAMESPACE

class Client : public QWidget {
    Q_OBJECT

  public:
    Client(QWidget* parent = nullptr);
    ~Client();

  private slots:
    void connectToServer();
    void sendMessage();
    void sendFile();
    void handleFileData();
    void displayError(QAbstractSocket::SocketError socketError);
    void handleFileInfo(const QByteArray& fileInfoData);

  private:
    Ui::Client* ui;
    QTcpSocket* tcpSocket;
    QString currentFileName;
    qint64 totalBytesToReceive;
    qint64 bytesReceived;
    QFile* receivedFile;
};

#endif  // CLIENT_H
