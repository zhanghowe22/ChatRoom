#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <map>
#include <string>

class ChatServer {
public:
    ChatServer(int port);
    ~ChatServer();
    void start();

private:
    int port;
    int server_fd;
    int epoll_fd;
    std::map<int, std::string> clients;

    void setupServer();
    void setNonBlocking(int sockfd);
    void broadcastMessage(const std::string &message, int exclude_fd);
    void run();
    void handleNewConnection();
    void handleClientMessage(int client_fd);
};

#endif // CHATSERVER_H
