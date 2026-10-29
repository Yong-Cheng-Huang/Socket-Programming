#include <iostream>
#include <string>

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

    sockaddr_in serverAddress = localAddress();

    std::cout << "Enter a lowercase sentence: ";

    std::string message;
    bool success = static_cast<bool>(
        std::getline(std::cin, message)
    );

    if (!success || message.size() > 1024) {
        std::cerr << "Input failed or exceeds 1024 bytes.\n";
        closeSocket(fd);
        stopNetwork();
        return 1;
    }

    // 2. 傳送小寫句子
    int length = static_cast<int>(message.size());

    auto sent = sendto(
        fd, message.data(), length, 0,
        reinterpret_cast<sockaddr*>(&serverAddress),
        sizeof(serverAddress)
    );

    if (sent < 0 || sent != length) {
        if (sent < 0) {
            printError("sendto");
        } else {
            std::cerr << "Unexpected datagram send length.\n";
        }

        closeSocket(fd);
        stopNetwork();
        return 1;
    }

    // 3. 接收 Server 回傳的大寫句子
    char buffer[2048];
    sockaddr_in senderAddress{};
    AddressLength senderLength = sizeof(senderAddress);

    auto received = recvfrom(
        fd, buffer, static_cast<int>(sizeof(buffer)), 0,
        reinterpret_cast<sockaddr*>(&senderAddress),
        &senderLength
    );

    success = (received >= 0);

    if (success) {
        success =
            senderAddress.sin_addr.s_addr ==
                serverAddress.sin_addr.s_addr &&
            senderAddress.sin_port == serverAddress.sin_port;

        if (success) {
            std::cout << "Uppercase response: ";
            std::cout.write(buffer, received);
            std::cout << '\n';
        } else {
            std::cerr << "Unexpected sender.\n";
        }
    } else {
        printError("recvfrom");
    }

    // 4. 關閉 Socket
    closeSocket(fd);
    stopNetwork();

    return success ? 0 : 1;
}