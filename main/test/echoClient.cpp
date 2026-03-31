#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

const char *ip = "127.0.0.1";
const int port = 8080;

int main() {
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);
    addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)))
        return 0;

    std::string msg;
    std::cin >> msg;
    write(sock, msg.c_str(), sizeof(msg.c_str()));
    char buff[1024];
    read(sock, buff, 1024);
    std::cout << buff;
    return 0;
}