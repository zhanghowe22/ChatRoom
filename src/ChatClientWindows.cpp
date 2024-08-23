#include "ChatClientWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>

ChatClientWindow::ChatClientWindow(QWidget *parent)
    : QMainWindow(parent), client(nullptr) {
    setupUI();
    connectSignalsAndSlots();
}

ChatClientWindow::~ChatClientWindow() {
    if (client) {
        delete client;
    }
}

void ChatClientWindow::setupUI() {
    messageDisplay = new QTextEdit(this);
    messageDisplay->setReadOnly(true);

    messageInput = new QLineEdit(this);

    serverIpInput = new QLineEdit(this);
    serverIpInput->setPlaceholderText("Server IP");

    serverPortInput = new QLineEdit(this);
    serverPortInput->setPlaceholderText("Server Port");

    connectButton = new QPushButton("Connect", this);
    sendButton = new QPushButton("Send", this);
    sendFileButton = new QPushButton("Send File", this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    QHBoxLayout *topLayout = new QHBoxLayout;
    QHBoxLayout *bottomLayout = new QHBoxLayout;

    topLayout->addWidget(serverIpInput);
    topLayout->addWidget(serverPortInput);
    topLayout->addWidget(connectButton);

    bottomLayout->addWidget(messageInput);
    bottomLayout->addWidget(sendButton);
    bottomLayout->addWidget(sendFileButton);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(messageDisplay);
    mainLayout->addLayout(bottomLayout);

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    setWindowTitle("Chat Client");
    resize(600, 400);
}

void ChatClientWindow::connectSignalsAndSlots() {
    connect(connectButton, &QPushButton::clicked, this, &ChatClientWindow::onConnectClicked);
    connect(sendButton, &QPushButton::clicked, this, &ChatClientWindow::onSendClicked);
    connect(sendFileButton, &QPushButton::clicked, this, &ChatClientWindow::onSendFileClicked);
}

void ChatClientWindow::onConnectClicked() {
    if (client) {
        delete client;
        client = nullptr;
    }

    QString serverIp = serverIpInput->text();
    int serverPort = serverPortInput->text().toInt();

    client = new ChatClient(serverIp.toStdString(), serverPort);

    QThread *clientThread = QThread::create([this] {
        client->start();
    });
    connect(clientThread, &QThread::finished, clientThread, &QThread::deleteLater);
    clientThread->start();

    connect(client, &ChatClient::messageReceived, this, &ChatClientWindow::displayReceivedMessage);

    QMessageBox::information(this, "Connected", "Connected to server!");
}

void ChatClientWindow::onSendClicked() {
    if (client) {
        QString message = messageInput->text();
        client->sendMessage(message.toStdString());
        messageInput->clear();
    }
}

void ChatClientWindow::onSendFileClicked() {
    if (client) {
        QString filePath = QFileDialog::getOpenFileName(this, "Select File to Send");
        if (!filePath.isEmpty()) {
            client->sendFile(filePath.toStdString());
        }
    }
}

void ChatClientWindow::displayReceivedMessage(const QString &message) {
    messageDisplay->append(message);
}
