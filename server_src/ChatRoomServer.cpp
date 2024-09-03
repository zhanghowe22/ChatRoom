#include "ChatRoomServer.h"

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

void ChatServer::broadcastFileInfo(const std::string &fileName, uint64_t fileSize, const int &exclude_fd)
{
    std::string fileInfoHeader = "FILE_INFO:" + fileName + ":" + std::to_string(fileSize) + "\n";
    for (const auto &client : m_clients)
    {
        if (client.first == exclude_fd)
        {
            continue;
        }
        send(client.first, fileInfoHeader.c_str(), fileInfoHeader.size(), 0);
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

    // 分块读取和发送文件数据
    const int bufferSize = 64 * 1024; // 64KB 缓冲区
    char buffer[bufferSize];

    for (const auto &client : m_clients)
    {
        int client_fd = client.first;

        // 排除发送者
        if (client_fd == exclude_fd)
        {
            continue;
        }

        // 发送文件头信息
        std::string fileHeader = "FILE:" + fileName + "\n";
        ssize_t headerSent = write(client_fd, fileHeader.c_str(), fileHeader.size());
        if (headerSent < 0)
        {
            std::cerr << "Failed to send file header to client " << client_fd << ": " << strerror(errno) << std::endl;
            continue;
        }

        // 重置文件流位置为文件开始
        file.clear(); // 清除文件流状态
        file.seekg(0, std::ios::beg);

        while (file.read(buffer, bufferSize))
        {
            ssize_t bytesSent = write(client_fd, buffer, file.gcount());
            if (bytesSent < 0)
            {
                std::cerr << "Error sending file data to client " << client_fd << ": " << strerror(errno) << std::endl;
                break;
            }
        }

        std::cout << "File " << fileName << " sent to client " << client_fd << " successfully." << std::endl;
    }

    file.close();
}

void ChatServer::receiveFileData(int client_fd, const std::string &fileName, int fileSize)
{
    std::ofstream file(fileName, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to open file for writing: " << fileName << std::endl;
        return;
    }

    int totalBytesReceived = 0;
    char buffer[BUFFER_SIZE];
    while (totalBytesReceived < fileSize)
    {
        int bytesReceived = read(client_fd, buffer, sizeof(buffer));
        if (bytesReceived > 0)
        {
            file.write(buffer, bytesReceived);
            totalBytesReceived += bytesReceived;
        }
        else if (bytesReceived == 0)
        {
            std::cout << "Client " << client_fd << " disconnected during file transfer." << std::endl;
            break;
        }
        else
        {
            std::cerr << "Error receiving file data from client " << client_fd << ": " << strerror(errno) << std::endl;
            break;
        }
    }

    file.close();
    if (totalBytesReceived == fileSize)
    {
        std::cout << "File received successfully from client " << client_fd << ": " << fileName << std::endl;
    }
    else
    {
        std::cerr << "File transfer incomplete or failed from client " << client_fd << std::endl;
    }
}

void ChatServer::handleFileRequest(int client_fd, const std::string &request) {
    // Extract file name from the request
    size_t colonPos = request.find(":");
    if (colonPos != std::string::npos) {
        std::string fileName = request.substr(colonPos + 1);
        sendFileToClients(fileName, client_fd);
    }
}

void ChatServer::handleClientMessage(int client_fd)
{
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        std::string receivedData(buffer, bytes_read);

        // Handle file info message
        if (receivedData.find("FILE_INFO:") == 0)
        {
            // Extract file name and size from message
            size_t firstColon = receivedData.find(":");
            size_t secondColon = receivedData.find(":", firstColon + 1);
            if (secondColon != std::string::npos)
            {
                std::string fileName = receivedData.substr(firstColon + 1, secondColon - firstColon - 1);
                std::string fileSizeStr = receivedData.substr(secondColon + 1);
                int fileSize = std::stoi(fileSizeStr);

                std::ofstream file(fileName, std::ios::binary);
                if (!file)
                {
                    std::cerr << "Failed to open file for writing: " << fileName << std::endl;
                    return;
                }
                
                file.write(buffer, bytes_read);
                
                // receiveFileData(client_fd, fileName, fileSize);

                broadcastFileInfo(fileName, fileSize, client_fd);
            }
        }
        // Handle file data
        else if (receivedData.find("REQUEST_FILE:") == 0)
        {
           handleFileRequest(client_fd, receivedData);
        }
        // Handle chat messages
        else
        {
            std::string message = m_clients[client_fd] + ": " + buffer;
            std::cout << message << std::endl;
            broadcastMessage(message, client_fd);
        }
    }
    else if (bytes_read == 0)
    {
        std::cout << m_clients[client_fd] << " disconnected." << std::endl;
        close(client_fd);
        m_clients.erase(client_fd);
    }
    else
    {
        std::cerr << "Error reading from client " << client_fd << ": " << strerror(errno) << std::endl;
        close(client_fd);
        m_clients.erase(client_fd);
    }
}
