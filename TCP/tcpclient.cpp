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
    Socket fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == BAD_SOCKET) {
        printError("socket");
        stopNetwork();
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // 2. 連線到 Server
    if (connect(fd,
                reinterpret_cast<sockaddr*>(&serverAddress),
                sizeof(serverAddress)) != 0) {
        printError("connect");
        closeSocket(fd);
        stopNetwork();
        return 1;
    }

    std::cout << "Connected to TCP Server!\n";
    std::cout << "Enter one line: ";

    std::string message;
    bool success = static_cast<bool>(std::getline(std::cin, message));

    if (!success || message.size() > 1024) {
        std::cerr << "Input failed or exceeds 1024 bytes.\n";
        closeSocket(fd);
        stopNetwork();
        return 1;
    }

    // 3. 傳送訊息，使用換行作為結束符號
    success = sendAll(fd, message + "\n");

    // 4. 接收回覆
    if (success) {
        std::string reply;
        success = receiveLine(fd, reply);

        if (success) {
            std::cout << "Echo: " << reply << '\n';
        }
    }

    // 5. 關閉 Socket
    closeSocket(fd);
    stopNetwork();

    return success ? 0 : 1;
}