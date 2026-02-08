// TcpServer.cpp
#include "TcpServer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ws2tcpip.h>
#include <iostream>
#include <string>

static void LogLine(const std::string& s) {
    std::cout << s << std::endl;
    ::OutputDebugStringA((s + "\n").c_str()); // VS: Debug -> Windows -> Output
}

static void LogWsaError(const char* where) {
    int e = WSAGetLastError();
    LogLine(std::string(where) + " WSAError=" + std::to_string(e));
}

TcpServer::TcpServer(std::string listenIp, uint16_t listenPort, size_t workerThreads)
    : _ip(std::move(listenIp)), _port(listenPort), _pool(workerThreads) {
}

TcpServer::~TcpServer() {
    Stop();
}

bool TcpServer::Start() {
    if (_running.exchange(true)) {
        LogLine("Start(): already running");
        return false;
    }

    LogLine("Start(): entering. PID=" + std::to_string(::GetCurrentProcessId()));
    LogLine("Start(): configured listen ip=" + (_ip.empty() ? std::string("<any>") : _ip) +
        " port=" + std::to_string(_port));

    // 1) Initialize WinSock (required before any socket calls on Windows).
    WSADATA wsa{};
    int wsaOk = WSAStartup(MAKEWORD(2, 2), &wsa);
    LogLine("WSAStartup() -> " + std::to_string(wsaOk));
    if (wsaOk != 0) {
        LogWsaError("WSAStartup failed.");
        _running.store(false);
        return false;
    }

    // 2) Resolve address/port we will bind to.
    addrinfo hints{};
    hints.ai_family = AF_INET;       // IPv4 only for simplicity
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* result = nullptr;
    std::string portStr = std::to_string(_port);
    const char* node = _ip.empty() ? nullptr : _ip.c_str();

    int gai = getaddrinfo(node, portStr.c_str(), &hints, &result);
    LogLine("getaddrinfo() -> " + std::to_string(gai));
    if (gai != 0 || !result) {
        LogWsaError("getaddrinfo failed.");
        WSACleanup();
        _running.store(false);
        return false;
    }

    // 3) Create a listening socket.
    _listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (_listenSocket == INVALID_SOCKET) {
        LogWsaError("socket failed.");
        freeaddrinfo(result);
        WSACleanup();
        _running.store(false);
        return false;
    }
    LogLine("socket() ok. listenSocket=" + std::to_string((uintptr_t)_listenSocket));

    // Allow quick restart after closing the server.
    BOOL reuse = TRUE;
    int sso = setsockopt(_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    LogLine("setsockopt(SO_REUSEADDR) -> " + std::to_string(sso));

    // 4) Bind the socket to the local address/port.
    int b = bind(_listenSocket, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);

    if (b == SOCKET_ERROR) {
        LogWsaError("bind failed.");
        closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;
        WSACleanup();
        _running.store(false);
        return false;
    }
    LogLine("bind() ok.");

    // 5) Start listening.
    int l = listen(_listenSocket, SOMAXCONN);
    if (l == SOCKET_ERROR) {
        LogWsaError("listen failed.");
        closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;
        WSACleanup();
        _running.store(false);
        return false;
    }
    LogLine("listen() ok.");

    LogLine("Listening on " + (_ip.empty() ? std::string("0.0.0.0") : _ip) + ":" + std::to_string(_port));

    // Run accept loop in the background so Start() returns immediately.
    _acceptThread = std::thread([this] { AcceptLoop(); });
    return true;
}

void TcpServer::Stop() {
    bool wasRunning = _running.exchange(false);
    if (!wasRunning) return;

    LogLine("Stop(): stopping...");

    // Closing the listen socket will unblock accept().
    if (_listenSocket != INVALID_SOCKET) {
        closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;
        LogLine("Stop(): listen socket closed");
    }

    if (_acceptThread.joinable()) {
        _acceptThread.join();
    }

    _pool.Stop();
    WSACleanup();

    LogLine("Stop(): done");
}

void TcpServer::AcceptLoop() {
    LogLine("AcceptLoop(): started");

    while (_running.load()) {
        SOCKET client = accept(_listenSocket, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            int e = WSAGetLastError();
            // 停止时 closesocket 会让 accept 失败，这是正常路径
            if (_running.load()) {
                LogLine("accept failed. WSAError=" + std::to_string(e));
            }
            else {
                LogLine("accept interrupted by stop. WSAError=" + std::to_string(e));
            }
            break;
        }

        LogLine("accept ok: clientSocket=" + std::to_string((uintptr_t)client));

        _pool.Enqueue([this, client] {
            HandleClient(client);
            });
    }

    LogLine("AcceptLoop(): exiting");
}

void TcpServer::HandleClient(SOCKET client) {
    LogLine("HandleClient(): begin clientSocket=" + std::to_string((uintptr_t)client));

    constexpr int BUF_SIZE = 4096;
    char buf[BUF_SIZE];

    // Simple echo loop: read bytes and send them back.
    while (_running.load()) {
        int n = recv(client, buf, BUF_SIZE, 0);
        if (n == 0) {
            LogLine("recv: client closed");
            break;
        }
        if (n < 0) {
            LogWsaError("recv failed.");
            break;
        }

        LogLine("recv bytes=" + std::to_string(n));

        int sent = 0;
        while (sent < n) {
            int m = send(client, buf + sent, n - sent, 0);
            if (m <= 0) {
                LogWsaError("send failed.");
                sent = -1;
                break;
            }
            sent += m;
        }
        if (sent < 0) break;
    }

    closesocket(client);
    LogLine("HandleClient(): end");
}
