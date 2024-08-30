#include <QApplication>

#include "chatClient.h"

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);
    Client client;
    client.show();
    return a.exec();
}
