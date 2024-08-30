#ifndef CLIENT_H
#define CLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressBar>

QT_BEGIN_NAMESPACE
namespace Ui { class Client; }
QT_END_NAMESPACE

class Client : public QWidget {
    Q_OBJECT

public:
    explicit Client(QWidget *parent = nullptr);
    ~Client();

private slots:
    void connectToServer();
    void onConnected();
    void onDisconnected();
    void onDataReceived();
    void sendMessage();
    void sendFile();
    void updateProgress(qint64 bytesSent);

private:
    Ui::Client *ui;
    QTcpSocket *tcpSocket;
    qint64 totalBytesToSend;
};

#endif // CLIENT_H
