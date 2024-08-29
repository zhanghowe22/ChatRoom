#include "mainwindow.h"

#include <QFileDialog>

#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow), m_client(new Client(this)) {
    ui->setupUi(this);
    connect(m_client, &Client::readyRead, this, &MainWindow::on_readyRead);
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::on_connectButton_clicked() {
    QString host = ui->hostEdit->text();
    quint16 port = ui->portEdit->text().toUInt();
    m_client->connectToServer(host, port);
}

void MainWindow::on_sendButton_clicked() {
    QString message = ui->messageEdit->text();
    m_client->sendMessage(message);
    ui->messageEdit->clear();
}

void MainWindow::on_fileButton_clicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select File");
    if (!filePath.isEmpty()) {
        m_client->sendFile(filePath);
    }
}

void MainWindow::on_readyRead() {
    QString message = m_client->readMessage();
    ui->chatBox->append(message);
}
