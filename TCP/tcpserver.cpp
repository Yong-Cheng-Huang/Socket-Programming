#include <iostream>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
using Socket = SOCKET;
constexpr Socket BAD_SOCKET = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstring>
using Socket = int;
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
#else
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
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

bool interrupted() {
#ifdef _WIN32
    return WSAGetLastError() == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

sockaddr_in localAddress() {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return address;
}

// TCP：處理一次 send() 未送出全部資料的情況
bool sendAll(Socket fd, const std::string& text) {
    if (text.size() > 1025) {
        return false;
    }

    int sent = 0;
    int length = static_cast<int>(text.size());

    while (sent < length) {
        auto n = send(fd, text.data() + sent, length - sent, 0);

        if (n < 0) {
            if (interrupted()) {
                continue;
            }
            printError("send");
            return false;
        }

        if (n == 0) {
            return false;
        }

        sent += static_cast<int>(n);
    }

    return true;
}

// TCP：持續接收，直到遇到換行
bool receiveLine(Socket fd, std::string& text) {
    text.clear();

    while (true) {
        char ch;
        auto n = recv(fd, &ch, 1, 0);

        if (n < 0) {
            if (interrupted()) {
                continue;
            }
            printError("recv");
            return false;
        }

        if (n == 0) {
            std::cerr << "Peer closed before newline.\n";
            return false;
        }

        if (ch == '\n') {
            return true;
        }

        if (text.size() >= 1024) {
            std::cerr << "Message exceeds 1024 bytes.\n";
            return false;
        }

        text.push_back(ch);
    }
}

int main() {
    if (!startNetwork()) {
        return 1;
    }

    // 1. 建立 TCP Socket
    Socket serverFd = socket(AF_INET, SOCK_STREAM, 0);

    if (serverFd == BAD_SOCKET) {
        printError("socket");
        stopNetwork();
        return 1;
    }

    // 2. 綁定地址
    sockaddr_in address = localAddress();

    if (bind(serverFd,
             reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) != 0) {
        printError("bind");
        closeSocket(serverFd);
        stopNetwork();
        return 1;
    }

    // 3. 開始監聽
    if (listen(serverFd, 5) != 0) {
        printError("listen");
        closeSocket(serverFd);
        stopNetwork();
        return 1;
    }

    std::cout << "TCP Server waiting..." << std::endl;

    // 4. 接受連線
    Socket clientFd = accept(serverFd, nullptr, nullptr);

    if (clientFd == BAD_SOCKET) {
        printError("accept");
        closeSocket(serverFd);
        stopNetwork();
        return 1;
    }

    // 5. 接收訊息
    std::string message;
    bool success = receiveLine(clientFd, message);

    if (success) {
        std::cout << "Received: " << message << '\n';

        // 6. 回傳原文
        success = sendAll(clientFd, message + "\n");
    }

    // 7. 關閉兩個 Socket
    closeSocket(clientFd);
    closeSocket(serverFd);
    stopNetwork();

    return success ? 0 : 1;
}