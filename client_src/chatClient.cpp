#include "chatClient.h"

#include <QDataStream>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>

#include "ui_chatClient.h"

Client::Client(QWidget* parent) :
    QWidget(parent), ui(new Ui::Client), tcpSocket(nullptr), totalBytesToReceive(0), bytesReceived(0) {
    ui->setupUi(this);

    // 初始状态下隐藏进度条
    ui->progressBar->hide();

    ui->statusLabel->setText("Disconnected");

    connect(ui->connectButton, &QPushButton::clicked, this, &Client::connectToServer);
    connect(ui->sendButton, &QPushButton::clicked, this, &Client::sendMessage);
    connect(ui->sendFileButton, &QPushButton::clicked, this, &Client::sendFile);
}

Client::~Client() { delete ui; }

void Client::handleSocketConnectionState() {
    if (tcpSocket->state() == QAbstractSocket::ConnectedState) {
        QMessageBox::information(this, "Already Connected", "Already connected to the server.");
    } else {
        tcpSocket->disconnectFromHost();
        tcpSocket->deleteLater();
        tcpSocket = nullptr;
    }
}

void Client::initializeSocket() {
    tcpSocket = new QTcpSocket(this);

    connect(tcpSocket, &QTcpSocket::connected, this, [this]() {
        ui->statusLabel->setText("Connected");
        QMessageBox::information(nullptr, "Connected", "Successfully connected to the server.");
    });

    connect(tcpSocket, &QTcpSocket::disconnected, this, [this]() {
        ui->statusLabel->setText("Disconnected");
        QMessageBox::information(nullptr, "Disconnected", "Disconnected from the server.");
    });

    connect(tcpSocket, &QTcpSocket::readyRead, this, [this]() { processReceivedData(); });

    // connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), this,
    //        &Client::displayError);
}

void Client::connectToServer() {
    // 检查套接字状态并处理连接逻辑
    if (tcpSocket) {
        handleSocketConnectionState();
    } else {
        initializeSocket();
    }

    QString hostAddress = ui->lineEditHost->text();
    quint16 port = ui->lineEditPort->text().toUShort();

    // 连接服务器
    tcpSocket->connectToHost(hostAddress, port);  // 检查套接字状态并处理连接逻辑
}

void Client::processReceivedData() {
    QByteArray data = tcpSocket->readAll();
    if (data.startsWith("FILE_INFO:")) {
        handleFileInfo(data);
    } else if (data.startsWith("FILE:")) {
        handleFileData(data);
    } else {
        ui->textBrowserChat->append("Server: " + QString(data));
    }
}

void Client::handleFileInfo(const QByteArray& fileInfoData) {
    QString fileInfoStr = QString::fromUtf8(fileInfoData);
    QStringList fileInfoParts = fileInfoStr.split(':');

    if (fileInfoParts.size() < 3)
        return;  // 数据格式不正确

    QString fileName = fileInfoParts[1];
    qint64 fileSize = fileInfoParts[2].toLongLong();

    receivedFile = new QFile(fileName);
    if (!receivedFile->open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Failed to open file for writing.");
        delete receivedFile;
        receivedFile = nullptr;
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        this, "File Transfer",
        QString("Do you want to download the file '%1' of size %2 bytes?").arg(fileName).arg(fileSize),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        QString request = QString("REQUEST_FILE:%1").arg(fileName);
        tcpSocket->write(request.toUtf8());
        tcpSocket->flush();
    }
}

void Client::sendMessage() {
    if (!tcpSocket || tcpSocket->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this, "Error", "Not connected to any server.");
        return;
    }

    QString message = ui->lineEditMessage->toPlainText();
    if (!message.isEmpty()) {
        ui->textBrowserChat->append("Me: " + message);

        tcpSocket->write(message.toUtf8());
        tcpSocket->flush();

        ui->lineEditMessage->clear();
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

    // 发送文件头
    QString fileHeader = QString("FILE_INFO:%1:%2:").arg(QFileInfo(file).fileName()).arg(file.size());
    QByteArray headerBytes = fileHeader.toUtf8();
    tcpSocket->write(headerBytes);
    tcpSocket->flush();  // 在发送文件内容前，先将头部发送完毕

    // 创建进度条
    // QProgressDialog progressDialog("Sending file...", "Cancel", 0, file.size(), this);
    // progressDialog.setWindowModality(Qt::WindowModal);
    // progressDialog.setMinimumDuration(0); // 立即显示进度条

    // 分块发送文件数据
    const qint64 bufferSize = 64 * 1024;  // 每次读取64KB
    qint64 bytesSent = 0;
    QByteArray buffer;

    while (!file.atEnd()) {
        // if (progressDialog.wasCanceled())
        // {
        //     QMessageBox::information(this, "Canceled", "File transfer was canceled");
        //     break;
        // }

        buffer = file.read(bufferSize);
        qint64 result = tcpSocket->write(buffer);
        if (result == -1) {
            QMessageBox::warning(this, "Error", "Failed to send data.");
            break;
        }
        tcpSocket->flush();
        bytesSent += result;
        // progressDialog.setValue(bytesSent);
        // QApplication::processEvents();  // 确保进度条更新
    }

    file.close();

    if (bytesSent == file.size()) {
        QMessageBox::information(this, "Success", "File sent successfully");
    } else if (bytesSent > 0) {
        QMessageBox::warning(this, "Warning", "File sent partially.");
    } else {
        QMessageBox::warning(this, "Error", "Failed to send the file.");
    }
}

QString Client::generateClientId() {
    QUuid uuid = QUuid::createUuid();
    return uuid.toString();  // 返回UUID字符串
}

void Client::handleFileData(const QByteArray& data) {
    if (!tcpSocket || !receivedFile || !receivedFile->isOpen())
        return;

    // 处理文件数据的头部信息
    if (data.startsWith("FILE:")) {
        if (!initializeFileReception(data))
            return;
    }

    writeFileData(data);
}

bool Client::initializeFileReception(const QByteArray& data) {
    QString fileInfoStr = QString::fromUtf8(data);
    QStringList fileInfoParts = fileInfoStr.split(':');

    if (fileInfoParts.size() < 4) {
        QMessageBox::warning(this, "Error", "Invalid file header format.");
        return false;
    }

    QString fileName = "client_" + fileInfoParts[1];
    qint64 fileSize = fileInfoParts[2].toLongLong();
    QString clientId = generateClientId();
    fileName = QString("%1_%2").arg(clientId, fileName);

    receivedFile = new QFile(fileName);
    if (!receivedFile->open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Failed to open file for writing.");
        delete receivedFile;
        receivedFile = nullptr;
        return false;
    }

    totalBytesToReceive = fileSize;
    bytesReceived = 0;
    return true;
}

void Client::writeFileData(const QByteArray& data) {
    qint64 bytesWritten = receivedFile->write(data);
    if (bytesWritten == -1) {
        QMessageBox::warning(this, "Error", "Failed to write file data.");
        cleanupFileReception();
        return;
    }

    bytesReceived += bytesWritten;
    if (bytesReceived >= totalBytesToReceive) {
        QMessageBox::information(this, "File Received", "File received completely.");
        cleanupFileReception();
    }
}

void Client::cleanupFileReception() {
    ui->progressBar->hide();
    if (receivedFile) {
        receivedFile->close();
        delete receivedFile;
        receivedFile = nullptr;
    }
}

void Client::displayError(QAbstractSocket::SocketError socketError) {
    QMessageBox::critical(this, "Socket Error", tcpSocket->errorString());
}
