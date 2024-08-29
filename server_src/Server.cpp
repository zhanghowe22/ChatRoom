#include "ChatRoomServer.h"

int main() {
    ChatServer server(8888);
    server.start();
    return 0;
}
