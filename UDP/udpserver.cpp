#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
using Socket = SOCKET;
using AddressLength = int;
constexpr Socket BAD_SOCKET = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
using Socket = int;
using AddressLength = socklen_t;
constexpr Socket BAD_SOCKET = -1;
#endif

bool startNetwork() {
#ifdef _WIN32
    WSADATA data{};
    int error = WSAStartup(MAKEWORD(2, 2), &data);
    if (error != 0) {
        std::cerr << "WSAStartup failed: " << error << '\n';
        return false;
    }
#endif
    return true;
}

void stopNetwork() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void closeSocket(Socket fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

void printError(const char* operation) {
#ifdef _WIN32
    std::cerr << operation << ": " << WSAGetLastError() << '\n';
#else
    std::cerr << operation << ": " << std::strerror(errno) << '\n';
#endif
}

sockaddr_in localAddress() {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return address;
}

int main() {
    if (!startNetwork()) {
        return 1;
    }

    // 1. 建立 UDP Socket
    Socket fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd == BAD_SOCKET) {
        printError("socket");
        stopNetwork();
        return 1;
    }

    // 2. 綁定地址
    sockaddr_in address = localAddress();

    if (bind(fd,
             reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) != 0) {
        printError("bind");
        closeSocket(fd);
        stopNetwork();
        return 1;
    }

    std::cout << "UDP Server waiting..." << std::endl;

    // 3. 接收 Client 傳送的句子
    char buffer[2048];
    sockaddr_in clientAddress{};
    AddressLength addressLength = sizeof(clientAddress);

    auto received = recvfrom(
        fd, buffer, static_cast<int>(sizeof(buffer)), 0,
        reinterpret_cast<sockaddr*>(&clientAddress),
        &addressLength
    );

    if (received < 0) {
        printError("recvfrom");
        closeSocket(fd);
        stopNetwork();
        return 1;
    }

    std::cout << "Received: ";
    std::cout.write(buffer, received);
    std::cout << '\n';

    // 4. 將英文小寫轉換成大寫
    for (int i = 0; i < received; i++) {
        unsigned char ch =
            static_cast<unsigned char>(buffer[i]);

        buffer[i] = static_cast<char>(std::toupper(ch));
    }

    std::cout << "Uppercase: ";
    std::cout.write(buffer, received);
    std::cout << '\n';

    // 5. 將大寫句子回傳 Client
    auto sent = sendto(
        fd, buffer, static_cast<int>(received), 0,
        reinterpret_cast<sockaddr*>(&clientAddress),
        addressLength
    );

    bool success = (sent == received);

    if (!success) {
        if (sent < 0) {
            printError("sendto");
        } else {
            std::cerr << "Unexpected datagram send length.\n";
        }
    }

    // 6. 關閉 Socket
    closeSocket(fd);
    stopNetwork();

    return success ? 0 : 1;
}
