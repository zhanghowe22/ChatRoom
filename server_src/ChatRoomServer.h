#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <map>
#include <string>

class ChatServer
{
public:
    ChatServer(int port);

    ~ChatServer();

    void start();

private:
    int port;

    int server_fd; // 服务端描述符

    int m_epoll_fd; // epoll实例

    std::map<int, std::string> m_clients; // map<描述符,客户端标识>

    void setupServer();

    // 设置为非阻塞，避免延迟引起的程序挂起
    void setNonBlocking(int sockfd);

    // 向除自己之外的客户端广播消息
    void broadcastMessage(const std::string &message, int exclude_fd);

    // 主循环 使用epoll监听事件，调用处理函数
    void run();

    // 处理客户端连接请求
    void handleNewConnection();

    // 处理客户端消息
    void handleClientMessage(int client_fd);
};

#endif // CHATSERVER_H
