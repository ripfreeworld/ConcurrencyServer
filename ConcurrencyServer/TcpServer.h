#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <atomic>
#include <thread>
#include "ThreadPool.h"

class TcpServer {
public:
    TcpServer(std::string listenIp, uint16_t listenPort, size_t workerThreads);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    bool Start();
    void Stop();

private:
    void AcceptLoop();
    void HandleClient(SOCKET client);

    std::string _ip;
    uint16_t _port;
    ThreadPool _pool;

    SOCKET _listenSocket = INVALID_SOCKET;
    std::atomic<bool> _running{ false };
    std::thread _acceptThread;
};
