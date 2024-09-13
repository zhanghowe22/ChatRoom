#include "ChatRoomServer.h"
#include <string>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define BACKLOG 10

ChatServer::ChatServer(int port) : port(port), server_fd(-1), m_epoll_fd(-1), m_fileSize(0) {}

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

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    // TODO: IP改为获取本机IP，端口可通过启动参数配置
    // address.sin_addr.s_addr = INADDR_ANY;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) < 0)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 创建epoll实例
    if ((m_epoll_fd = epoll_create1(0)) == -1)
    {
        perror("epoll_create1");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    addToEpoll(server_fd);

    std::cout << "Server [" << inet_ntoa(address.sin_addr) << "] is listening on port " << port << std::endl;
}

void ChatServer::addToEpoll(int fd)
{
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = fd;
    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1)
    {
        perror("epoll_ctl: addToEpoll failed");
        close(fd);
        exit(EXIT_FAILURE);
    }
}

void ChatServer::run()
{
    struct epoll_event events[MAX_EVENTS];

    while (true)
    {
        int n = epoll_wait(m_epoll_fd, events, MAX_EVENTS, -1);
        if(n == -1) {
            perror("epoll_wait failed");
            break;
        }

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

void ChatServer::setNonBlocking(int sockfd)
{
    int opts = fcntl(sockfd, F_GETFL);
    if (opts < 0)
    {
        perror("fcntl(F_GETFL) failed");
        exit(1);
    }

    if (fcntl(sockfd, F_SETFL, opts | O_NONBLOCK) < 0)
    {
        perror("fcntl(F_SETFL) failed");
        exit(1);
    }
}

void ChatServer::broadcastMessage(const std::string &message, int exclude_fd)
{
    for (const auto &client : m_clients)
    {
        if (client.first != exclude_fd)
        {
            send(client.first, message.c_str(), message.size(), 0);
        }
    }
}

void ChatServer::handleNewConnection()
{
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    if (new_socket < 0)
    {
        perror("accept failed");
        return;
    }

    setNonBlocking(new_socket);

    addToEpoll(new_socket);

    std::string client_id = "Client " + std::to_string(new_socket);
    m_clients[new_socket] = client_id;
    std::cout << client_id << " connected." << std::endl;

    broadcastClientList();
}

void ChatServer::handleClientMessage(int client_fd)
{
    char buffer[BUFFER_SIZE] = {0};

    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0)
    {
        std::string receivedData(buffer, bytes_read);

        // 当接收到文件消息对应的消息头
        if (receivedData.find("FILE_INFO:") == 0)
        {
            processFileInfo(client_fd, receivedData);
        }
        // 处理文件内容
        else if (receivedData.find("REQUEST_FILE:") == 0)
        {
            handleFileRequest(client_fd, receivedData);
        }
        // 处理对话消息
        else
        {
            std::string message = m_clients[client_fd] + ": " + buffer;
            std::cout << message << std::endl;
            broadcastMessage(message, client_fd);
        }
    }
    else if (bytes_read == 0)
    {
        disconnectClient(client_fd);
    }
    else
    {
        std::cerr << "Error reading from client " << client_fd << ": " << strerror(errno) << std::endl;
        disconnectClient(client_fd);
    }
}

void ChatServer::processFileInfo(int client_fd, const std::string &receivedData)
{
    size_t firstColon = receivedData.find(":");
    size_t secondColon = receivedData.find(":", firstColon + 1);
    if (secondColon != std::string::npos)
    {
        std::string fileName = receivedData.substr(firstColon + 1, secondColon - firstColon - 1);
        std::string fileSizeStr = receivedData.substr(secondColon + 1);
        m_fileSize = std::stoi(fileSizeStr);
        receiveFileData(client_fd, fileName, m_fileSize);
        broadcastFileInfo(fileName, m_fileSize, client_fd);
    }
}

void ChatServer::broadcastFileInfo(const std::string &fileName, uint64_t fileSize, const int &exclude_fd)
{
    std::string fileInfoHeader = "FILE_INFO:" + fileName + ":" + std::to_string(fileSize) + "\n";
    for (const auto &client : m_clients)
    {
        if (client.first != exclude_fd)
        {
            send(client.first, fileInfoHeader.c_str(), fileInfoHeader.size(), 0);
        }
    }
}

void ChatServer::sendFileToClients(const std::string &fileName, const int &exclude_fd)
{
    std::ifstream file(fileName, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to open file for reading: " << fileName << std::endl;
        return;
    }

    const int bufferSize = 64 * 1024; // 64KB 缓冲区
    char buffer[bufferSize];

    for (const auto &client : m_clients)
    {
        int client_fd = client.first;

        if (client_fd != exclude_fd)
            continue;

        // 发送文件头信息
        std::string fileHeader = "FILE:" + fileName + ":" + std::to_string(m_fileSize) + ":";
        if (!sendData(client_fd, fileHeader))
            continue;

        // 强制刷新文头信息到缓冲
        int flag = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

        // 发送文件数据
        while (!file.eof())
        {
            file.read(buffer, bufferSize);
            if (!sendData(client_fd, std::string(buffer, file.gcount())))
                break;
        }

        std::cout << "File " << fileName << " sent to client " << client_fd << " successfully." << std::endl;

        // 重置文件流位置为文件开始
        file.clear();
        file.seekg(0, std::ios::beg);
    }

    file.close();
}

void ChatServer::receiveFileData(int client_fd, const std::string &fileName, int fileSize)
{
    std::ofstream outFile(fileName, std::ios::binary | std::ios::trunc);
    if (!outFile)
    {
        std::cerr << "Cannot open file: " << fileName << std::endl;
        return;
    }

    char buffer[BUFFER_SIZE];
    uint64_t receivedSize = 0;

    while (receivedSize < fileSize)
    {
        int bytesRead = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytesRead > 0)
        {
            outFile.write(buffer, bytesRead);
            receivedSize += bytesRead;
        }
        else if (bytesRead == 0)
        {
            std::cout << "File reception completed." << std::endl;
            break;
        }
        else
        {
            std::cerr << "Error receiving file data: " << strerror(errno) << std::endl;
            break;
        }
    }

    outFile.close();
}

void ChatServer::handleFileRequest(int client_fd, const std::string &request)
{
    // 从请求消息中解析文件名
    size_t colonPos = request.find(":");
    if (colonPos != std::string::npos)
    {
        std::string fileName = request.substr(colonPos + 1);
        sendFileToClients(fileName, client_fd);
    }
}

bool ChatServer::sendData(int client_fd, const std::string &data)
{
    ssize_t totalSent = 0;
    ssize_t dataSize = data.size();
    const char *dataPtr = data.c_str();

    while (totalSent < dataSize)
    {
        ssize_t sent = send(client_fd, dataPtr + totalSent, dataSize - totalSent, 0);
        if (sent < 0)
        {
            std::cerr << "Failed to send data to client " << client_fd << ": " << strerror(errno) << std::endl;
            return false;
        }
        totalSent += sent;
    }
    return true;
}

void ChatServer::broadcastClientList()
{
    std::string clientListMessage = "CLIENT_LIST:";
    for(const auto &client : m_clients)
    {
        clientListMessage += client.second + ","; // 添加客户端ID
    }
    clientListMessage.pop_back(); // 去除最后一个逗号

    for(const auto &client : m_clients) {
        send(client.first, clientListMessage.c_str(), clientListMessage.size(), 0);
    }
}

void ChatServer::disconnectClient(int client_fd)
{
    std::cout << "Client " << client_fd << " disconnected." << std::endl;
    close(client_fd);
    epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    m_clients.erase(client_fd);

    broadcastClientList();
}