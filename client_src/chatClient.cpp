#include "chatClient.h"
#include "ui_chatClient.h"

Client::Client(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Client),
    tcpSocket(nullptr),
    totalBytesToSend(0) {
    ui->setupUi(this);

    // 连接按钮槽函数
    connect(ui->connectButton, &QPushButton::clicked, this, &Client::connectToServer);
    connect(ui->sendButton, &QPushButton::clicked, this, &Client::sendMessage);
    connect(ui->sendFileButton, &QPushButton::clicked, this, &Client::sendFile);
}

Client::~Client() {
    delete ui;
    if (tcpSocket) {
        tcpSocket->disconnectFromHost();
        tcpSocket->deleteLater();
    }
}

void Client::connectToServer() {
    QString hostAddress = ui->lineEditHost->text();
    quint16 port = ui->lineEditPort->text().toUShort();

    tcpSocket = new QTcpSocket(this);
    connect(tcpSocket, &QTcpSocket::connected, this, &Client::onConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &Client::onDisconnected);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &Client::onDataReceived);
    connect(tcpSocket, &QTcpSocket::bytesWritten, this, &Client::updateProgress);

    tcpSocket->connectToHost(hostAddress, port);
}

void Client::onConnected() {
    ui->statusLabel->setText("Connected to server");
}

void Client::onDisconnected() {
    ui->statusLabel->setText("Disconnected from server");
}

void Client::onDataReceived() {
    QByteArray data = tcpSocket->readAll();
    
    if (data.startsWith("FILE:")) {
        QString fileName = data.split(':')[1];
        QByteArray fileContent = data.mid(data.indexOf(':', 5) + 1);

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "File Received", "Download file " + fileName + "?",
                                      QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            QString savePath = QFileDialog::getSaveFileName(this, "Save File", fileName);
            QFile file(savePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(fileContent);
                file.close();
                QMessageBox::information(this, "Success", "File downloaded successfully!");
            }
        }
    } else {
        ui->textBrowserChat->append(QString::fromUtf8(data));
    }
}

void Client::sendMessage() {
    QString message = ui->lineEditMessage->text();
    if (!message.isEmpty()) {
        tcpSocket->write(message.toUtf8());
        ui->textBrowserChat->append("You: " + message);
        ui->lineEditMessage->clear();
    }
}

void Client::sendFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Select File to Send");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray fileData = file.readAll();
            tcpSocket->write("FILE:" + file.fileName().toUtf8() + ":" + fileData);
            file.close();
        }
    }
}

void Client::updateProgress(qint64 bytesSent) {
    ui->progressBar->setValue((bytesSent * 100) / totalBytesToSend);
}
