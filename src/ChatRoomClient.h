#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QObject>
#include <string>
#include <mutex>

class ChatClient : public QObject {
    Q_OBJECT

public:
    ChatClient(const std::string &server_ip, int server_port);
    ~ChatClient();
    void start();
    void sendMessage(const std::string &message);
    void sendFile(const std::string &file_path);

signals:
    void messageReceived(const QString &message);

private:
    std::string server_ip;
    int server_port;
    int sockfd;
    std::mutex mtx;  // 用于线程同步

    void connectToServer();
    void handleReceive();
};

#endif // CHATCLIENT_H
