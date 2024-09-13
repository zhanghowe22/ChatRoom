#ifndef CLIENT_H
#define CLIENT_H

#include <QFile>
#include <QTcpSocket>
#include <QWidget>
#include <QUuid>

QT_BEGIN_NAMESPACE
namespace Ui
{
  class Client;
}
QT_END_NAMESPACE

class Client : public QWidget
{
  Q_OBJECT

public:
  Client(QWidget *parent = nullptr);
  ~Client();

private:
  // 处理新连接
  void handleSocketConnectionState();

  // 初始化socket
  void initializeSocket();

  // 处理接收到的数据
  void processReceivedData();

  // 初始化文件接收
  bool initializeFileReception(const QByteArray& data);

  // 写文件
  void writeFileData(const QByteArray& data);

  // 处理文件数据
  void handleFileData(const QByteArray& data);

  // 清理文件接收
  void cleanupFileReception();

  // 测试时会将不同客户端的文件存在同一个路径下，所以增加id区分不同客户端的文件
  QString generateClientId();

  // 更新连接状态
  void updateStatusLabel(bool connected);

  void handleFileInfo(const QByteArray &fileInfoData);

  void handleClientList(const QByteArray& data);

private slots:
  void connectToServer();
  void disconnectFromServer();
  void sendMessage();
  void sendFile();
  void displayError(QAbstractSocket::SocketError socketError);

private:
  Ui::Client *ui;
  QTcpSocket *tcpSocket;
  QString currentFileName;
  qint64 totalBytesToReceive;
  qint64 bytesReceived;
  QFile *receivedFile;
};

#endif // CLIENT_H
