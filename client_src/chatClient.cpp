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

    // Initial state of the progress bar
    ui->progressBar->hide();

    connect(ui->connectButton, &QPushButton::clicked, this, &Client::connectToServer);
    connect(ui->sendButton, &QPushButton::clicked, this, &Client::sendMessage);
    connect(ui->sendFileButton, &QPushButton::clicked, this, &Client::sendFile);
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
            if (data.startsWith("FILE_INFO:")) {
                handleFileInfo(data);
            } else if (data.startsWith("FILE:")) {
                handleFileData(data);
            } else {
                ui->textBrowserChat->append("Server: " + QString(data));
            }
        }
    });
    // connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), this,
    // &Client::displayError);

    tcpSocket->connectToHost(hostAddress, port);
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

    // 发送文件头
    QString fileHeader = QString("FILE_INFO:%1:%2:").arg(QFileInfo(file).fileName()).arg(file.size());
    QByteArray headerBytes = fileHeader.toUtf8();
    tcpSocket->write(headerBytes);
    tcpSocket->flush();  // 在发送文件内容前，先将头部发送完毕

    // 创建进度条
    //    QProgressDialog progressDialog("Sending file...", "Cancel", 0, file.size(), this);
    //    progressDialog.setWindowModality(Qt::WindowModal);
    //    progressDialog.setMinimumDuration(0);  // 立即显示进度条

    // 分块发送文件数据
    const qint64 bufferSize = 64 * 1024;  // 每次读取64KB
    qint64 bytesSent = 0;
    QByteArray buffer;

    while (!file.atEnd()) {
        //        if (progressDialog.wasCanceled()) {
        //            QMessageBox::information(this, "Canceled", "File transfer was canceled");
        //            break;
        //        }

        buffer = file.read(bufferSize);
        qint64 result = tcpSocket->write(buffer);
        if (result == -1) {
            QMessageBox::warning(this, "Error", "Failed to send data.");
            break;
        }
        tcpSocket->flush();
        bytesSent += result;
        //        progressDialog.setValue(bytesSent);
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
	return uuid.toString(); // 返回UUID字符串
}

void Client::handleFileData(const QByteArray& data) {
	if (!tcpSocket || !receivedFile || !receivedFile->isOpen()) {
		return;
	}

	// 确保数据的头部以 "FILE" 开头
	if (data.startsWith("FILE:")) {
		// 提取文件头部信息
		QString fileInfoStr = QString::fromUtf8(data);

		QStringList fileInfoParts = fileInfoStr.split(':');

		if (fileInfoParts.size() < 4) {
			QMessageBox::warning(this, "Error", "Invalid file header format.");
			return;
		}

		// 计算文件头的长度
        int headerLength = fileInfoParts[0].size() + 1 + fileInfoParts[1].size() + 1 + fileInfoParts[2].size() + 1;

		QString fileName = "clinet_" + fileInfoParts[1];
		qint64 fileSize = fileInfoParts[2].toLongLong();

		// 初始化文件接收设置
		totalBytesToReceive = fileSize;
		bytesReceived = 0;

        // 文件名增加客户端标识(测试时不同客户端会保存在同一路径下，所以增加标识)
        QString clientId = generateClientId();
        fileName = QString("%1_%2").arg(clientId, fileName);

		receivedFile = new QFile(fileName);
		if (!receivedFile->open(QIODevice::WriteOnly)) {
			QMessageBox::warning(this, "Error", "Failed to open file for writing.");
			delete receivedFile;
			receivedFile = nullptr;
			return;
		}

		// 剩余的数据是文件内容
		QByteArray fileData = data.mid(headerLength, fileSize); // 获取剩余的文件数据
		qint64 bytesWritten = receivedFile->write(fileData);

		if (bytesWritten == -1) {
			QMessageBox::warning(this, "Error", "Failed to write file data.");
			receivedFile->close();
			delete receivedFile;
			receivedFile = nullptr;
			return;
		}

		bytesReceived += bytesWritten;

		// 如果已经接收了完整的文件，完成文件接收
		if (bytesReceived >= totalBytesToReceive) {
			QMessageBox::information(this, "File Received", "File received completely.");
			ui->progressBar->hide();
			receivedFile->close();
			delete receivedFile;
			receivedFile = nullptr;
			return;
		}
	}
	else {
		// 处理接收到的数据块（可能是文件内容的后续部分）
		QByteArray fileData = data;
		qint64 bytesWritten = receivedFile->write(fileData);

		if (bytesWritten == -1) {
			QMessageBox::warning(this, "Error", "Failed to write file data.");
			receivedFile->close();
			delete receivedFile;
			receivedFile = nullptr;
			return;
		}

		bytesReceived += bytesWritten;

		if (bytesReceived >= totalBytesToReceive) {
			QMessageBox::information(this, "File Received", "File received completely.");
			receivedFile->close();
			delete receivedFile;
			receivedFile = nullptr;
			return;
		}
	}
}


void Client::displayError(QAbstractSocket::SocketError socketError) {
    QMessageBox::critical(this, "Socket Error", tcpSocket->errorString());
}
