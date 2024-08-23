#include "ChatRoomClient.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <QThread>
#include <QDebug>

#define BUFFER_SIZE 1024

ChatClient::ChatClient(const std::string &server_ip, int server_port)
    : server_ip(server_ip), server_port(server_port), sockfd(-1) {}

ChatClient::~ChatClient() {
    if (sockfd != -1) close(sockfd);
}

void ChatClient::start() {
    connectToServer();
    handleReceive();
}

void ChatClient::connectToServer() {
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }

    qDebug() << "Connected to server at " << QString::fromStdString(server_ip) << ":" << server_port;
}

void ChatClient::handleReceive() {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            QString message(buffer);
            emit messageReceived(message);
        } else if (bytes_received == 0) {
            qDebug() << "Server disconnected.";
            break;
        } else {
            perror("recv");
        }
    }
}

void ChatClient::sendMessage(const std::string &message) {
    std::lock_guard<std::mutex> lock(mtx);
    send(sockfd, message.c_str(), message.length(), 0);
}

void ChatClient::sendFile(const std::string &file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        qDebug() << "Failed to open file:" << QString::fromStdString(file_path);
        return;
    }

    // 提示其他客户端准备接收文件
    std::string file_name = file_path.substr(file_path.find_last_of("/\\") + 1);
    sendMessage("FILE:" + file_name);

    // 逐块发送文件内容
    char buffer[BUFFER_SIZE];
    while (file.read(buffer, sizeof(buffer))) {
        send(sockfd, buffer, file.gcount(), 0);
    }
    if (file.gcount() > 0) {
        send(sockfd, buffer, file.gcount(), 0);
    }

    file.close();
    qDebug() << "File sent:" << QString::fromStdString(file_name);
}
