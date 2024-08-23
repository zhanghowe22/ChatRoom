#ifndef CHATCLIENTWINDOW_H
#define CHATCLIENTWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>
#include "ChatRoomClient.h"

class ChatClientWindow : public QMainWindow {
    Q_OBJECT

public:
    ChatClientWindow(QWidget *parent = nullptr);
    ~ChatClientWindow();

private slots:
    void onConnectClicked();
    void onSendClicked();
    void onSendFileClicked();
    void displayReceivedMessage(const QString &message);

private:
    ChatClient *client;
    QTextEdit *messageDisplay;
    QLineEdit *messageInput;
    QLineEdit *serverIpInput;
    QLineEdit *serverPortInput;
    QPushButton *connectButton;
    QPushButton *sendButton;
    QPushButton *sendFileButton;

    void setupUI();
    void connectSignalsAndSlots();
};

#endif // CHATCLIENTWINDOW_H
