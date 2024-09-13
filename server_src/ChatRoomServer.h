#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <map>
#include <fcntl.h>
#include <cstring>
#include <fstream>
#include <vector>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <fcntl.h>
#include <netinet/tcp.h>

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

    std::string m_fileName;

    int m_fileSize;

    int m_fileReceivedBytes;

    // 服务端启动监听
    void setupServer();

    // 添加到Epoll队列
    void addToEpoll(int fd);

    // 主循环 使用epoll监听事件(新的客户端连接或客户端消息)，调用对应处理函数 
    void run();

    // 设置为非阻塞，避免延迟引起的程序挂起
    void setNonBlocking(int sockfd);

    // 向除自己之外的客户端广播消息
    void broadcastMessage(const std::string &message, int exclude_fd);

    // 处理客户端连接请求
    void handleNewConnection();

    void handleFileRequest(int client_fd, const std::string &request);

    // 处理客户端消息
    void handleClientMessage(int client_fd);

    // 处理文件信息头
    void processFileInfo(int client_fd, const std::string &receivedData);

    // 接收文件数据
    void receiveFileData(int client_fd, const std::string &fileName, int fileSize);

    // 广播文件信息
    void broadcastFileInfo(const std::string &fileName, uint64_t fileSize, const int &exclude_fd);

    // 发送文件到对应客户端
    void sendFileToClients(const std::string &fileName, const int &exclude_fd);

    // 断开客户端连接
    void disconnectClient(int client_fd);

    // 向指定socket发送数据
    bool sendData(int client_fd, const std::string &data);

    // 广播当前已连接的客户端列表
    void broadcastClientList();
};

#endif // CHATSERVER_H
