#include "ChatRoomServer.h"
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <fcntl.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

ChatServer::ChatServer(int port) : port(port), server_fd(-1), m_epoll_fd(-1) {}

ChatServer::~ChatServer()
{
    if (server_fd != -1)
        close(server_fd);
    if (m_epoll_fd != -1)
        close(m_epoll_fd);
}

void ChatServer::start()
{
    setupServer();
    run();
}

void ChatServer::setupServer()
{
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    m_epoll_fd = epoll_create1(0);
    if (m_epoll_fd == -1)
    {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
    {
        perror("epoll_ctl: server_fd");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server [" << inet_ntoa(address.sin_addr) << "] is listening on port " << port << std::endl;
}

void ChatServer::setNonBlocking(int sockfd)
{
    int opts = fcntl(sockfd, F_GETFL);
    if (opts < 0)
    {
        perror("fcntl(F_GETFL)");
        exit(1);
    }
    opts = (opts | O_NONBLOCK);
    if (fcntl(sockfd, F_SETFL, opts) < 0)
    {
        perror("fcntl(F_SETFL)");
        exit(1);
    }
}

void ChatServer::broadcastMessage(const std::string &message, int exclude_fd)
{
    for (auto &client : m_clients)
    {
        if (client.first != exclude_fd)
        {
            send(client.first, message.c_str(), message.size(), 0);
        }
    }
}

void ChatServer::run()
{
    struct epoll_event events[MAX_EVENTS];

    while (true)
    {
        int n = epoll_wait(m_epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < n; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                handleNewConnection();
            }
            else
            {
                handleClientMessage(events[i].data.fd);
            }
        }
    }
}

void ChatServer::handleNewConnection()
{
    int new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0)
    {
        perror("accept");
        return;
    }

    setNonBlocking(new_socket);

    struct epoll_event event;
    event.data.fd = new_socket;
    event.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1)
    {
        perror("epoll_ctl: new_socket");
        return;
    }

    std::string client_id = "Client " + std::to_string(new_socket);
    m_clients[new_socket] = client_id;
    std::cout << client_id << " connected." << std::endl;
}

void ChatServer::handleClientMessage(int client_fd)
{
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        std::string message = m_clients[client_fd] + ": " + buffer;
        std::cout << message << std::endl;
        broadcastMessage(message, client_fd);
    }
    else if (bytes_read == 0)
    {
        std::cout << m_clients[client_fd] << " disconnected." << std::endl;
        close(client_fd);
        m_clients.erase(client_fd);
    }
}
