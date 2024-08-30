#include "chatClient.h"

#include <QDataStream>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

#include "ui_chatClient.h"

Client::Client(QWidget* parent) :
    QWidget(parent), ui(new Ui::Client), tcpSocket(nullptr), totalBytesToReceive(0), bytesReceived(0) {
    ui->setupUi(this);

    // Initial state of the progress bar
    ui->progressBar->hide();

    connect(ui->connectButton, &QPushButton::clicked, this, &Client::connectToServer);
    connect(ui->sendButton, &QPushButton::clicked, this, &Client::sendMessage);
    connect(ui->sendFileButton, &QPushButton::clicked, this, &Client::sendFile);
    connect(ui->receiveFileButton, &QPushButton::clicked, this, &Client::receiveFile);
}

Client::~Client() { delete ui; }

void Client::connectToServer() {
    if (tcpSocket) {
        if (tcpSocket->state() == QAbstractSocket::ConnectedState) {
            QMessageBox::information(this, "Already Connected", "Already connected to the server.");
            return;
        } else {
            tcpSocket->disconnectFromHost();
            tcpSocket->deleteLater();
        }
    }

    QString hostAddress = ui->lineEditHost->text();
    quint16 port = ui->lineEditPort->text().toUShort();

    tcpSocket = new QTcpSocket(this);

    connect(tcpSocket, &QTcpSocket::connected, this,
            []() { QMessageBox::information(nullptr, "Connected", "Successfully connected to the server."); });
    connect(tcpSocket, &QTcpSocket::disconnected, this,
            []() { QMessageBox::information(nullptr, "Disconnected", "Disconnected from the server."); });
    connect(tcpSocket, &QTcpSocket::readyRead, this, [this]() {
        if (tcpSocket->bytesAvailable() > 0) {
            QByteArray data = tcpSocket->readAll();
            if (data.startsWith("FILE:")) {
                handleFileHeader(data);
            } else {
                ui->textBrowserChat->append("Server: " + QString(data));
            }
        }
    });
    // connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), this,
    // &Client::displayError);

    tcpSocket->connectToHost(hostAddress, port);
}

void Client::sendMessage() {
    if (!tcpSocket || tcpSocket->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this, "Error", "Not connected to any server.");
        return;
    }

    QString message = ui->textEditMessage->toPlainText();
    if (!message.isEmpty()) {
        tcpSocket->write(message.toUtf8());
        ui->textEditMessage->clear();
    }
}

void Client::sendFile() {
    if (!tcpSocket || tcpSocket->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this, "Error", "Not connected to any server.");
        return;
    }

    QString fileName = QFileDialog::getOpenFileName(this, "Open File");
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Failed to open file.");
        return;
    }

    // Send file header
    QByteArray fileHeader;
    QDataStream headerStream(&fileHeader, QIODevice::WriteOnly);
    QFileInfo fileInfo(file);
    headerStream << "FILE:" << fileInfo.fileName() << file.size();
    tcpSocket->write(fileHeader);

    // Send file data
    QByteArray fileData = file.readAll();
    tcpSocket->write(fileData);
    file.close();
}

void Client::receiveFile() {
    if (!tcpSocket || tcpSocket->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this, "Error", "Not connected to any server.");
        return;
    }

    QString saveFileName = QFileDialog::getSaveFileName(this, "Save File As");
    if (saveFileName.isEmpty()) {
        return;
    }

    receivedFile = new QFile(saveFileName);
    if (!receivedFile->open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Failed to open file for writing.");
        delete receivedFile;
        receivedFile = nullptr;
        return;
    }

    connect(tcpSocket, &QTcpSocket::readyRead, this, &Client::handleFileData);
}

void Client::handleFileHeader(const QByteArray& headerData) {
    QDataStream headerStream(headerData);
    QString fileName;
    qint64 fileSize;
    headerStream >> fileName >> fileSize;

    totalBytesToReceive = fileSize;
    bytesReceived = 0;

    QFileInfo fileInfo(fileName);
    QFile* file = new QFile(fileInfo.absoluteFilePath());
    if (file->open(QIODevice::WriteOnly)) {
        ui->progressBar->setMaximum(totalBytesToReceive);
        ui->progressBar->show();
        currentFileName = fileInfo.absoluteFilePath();
    } else {
        QMessageBox::warning(this, "Error", "Failed to open file for writing.");
        delete file;
        return;
    }

    connect(tcpSocket, &QTcpSocket::readyRead, this, &Client::handleFileData);
}

void Client::handleFileData() {
    if (!tcpSocket || !tcpSocket->bytesAvailable()) {
        return;
    }

    QByteArray fileData = tcpSocket->readAll();
    QFile file(currentFileName);
    if (file.open(QIODevice::Append)) {
        file.write(fileData);
        file.close();

        bytesReceived += fileData.size();
        ui->progressBar->setValue(bytesReceived);

        if (bytesReceived == totalBytesToReceive) {
            ui->progressBar->hide();
            QMessageBox::information(this, "File Received", "File received completely.");
            disconnect(tcpSocket, &QTcpSocket::readyRead, this, &Client::handleFileData);
        }
    } else {
        QMessageBox::warning(this, "Error", "Failed to write file data.");
    }
}

void Client::displayError(QAbstractSocket::SocketError socketError) {
    QMessageBox::critical(this, "Socket Error", tcpSocket->errorString());
}
