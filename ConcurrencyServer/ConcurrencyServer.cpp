#include "TcpServer.h"
#include <iostream>
#include <thread>
#include <algorithm>

int main() {
    std::cout << "Program start\n";
    std::cout << "PID=" << GetCurrentProcessId() << std::endl;
    size_t threads = std::max<size_t>(2u, std::thread::hardware_concurrency());
    TcpServer server("", 8080, threads);

    std::cout << "Calling Start()\n";
    bool ok = server.Start();
    std::cout << "Start() returned: " << ok << "\n";
    if (!ok) return 1;

    std::cout << "Server is running. Press Enter to stop...\n";
    std::cin.get();
    server.Stop();
    return 0;
}
