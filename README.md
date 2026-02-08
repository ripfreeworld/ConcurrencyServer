# ConcurrencyServer

This project is a small Windows TCP echo server written in C++. It accepts many client connections and uses a simple thread pool to handle them concurrently.

## What it does

- Listens on a TCP port (default: 8080).
- Accepts client connections.
- For each client, reads data and sends the same data back (echo).

## Test with a simple client

This creates a TCP client, sends a line, and prints what the server echoes:

```powershell
$client = New-Object System.Net.Sockets.TcpClient("127.0.0.1", 8080)
$stream = $client.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$writer.AutoFlush = $true
$writer.WriteLine("hello from client")
$reader = New-Object System.IO.StreamReader($stream)
$reader.ReadLine()
$client.Close()
```

You should see the same text printed by the last line.

## Project structure

- `ConcurrencyServer.cpp` - Program entry point (`main`).
- `TcpServer.h/.cpp` - TCP server implementation (WinSock setup, accept loop, client handling).
- `ThreadPool.h/.cpp` - Simple thread pool used to run client handlers.

## Key concepts

- **WinSock**: The Windows socket API. You must call `WSAStartup` before using sockets and `WSACleanup` when done.
- **Listening socket**: A socket used only to accept new clients.
- **Accept loop**: A loop that calls `accept()` to get new client sockets.
- **Thread pool**: A fixed number of worker threads that take tasks from a queue. This avoids creating a new thread for every client.
- **Echo server**: A server that sends back whatever it receives. Great for learning networking basics.

## How to change the port

Open `ConcurrencyServer.cpp` and change the port number in the `TcpServer` constructor:

```cpp
TcpServer server("", 8080, threads);
```

## How it works

1. `main` creates a `TcpServer` and calls `Start()`.
2. `Start()` initializes WinSock, creates/binds/listens on a socket.
3. A background thread runs `AcceptLoop()`.
4. For each client, a task is queued to the thread pool.
5. The task runs `HandleClient()` which echoes data until the client closes.

## Known limitations (kept simple on purpose)

- IPv4 only.
- No TLS/SSL encryption.
- No advanced shutdown for connected clients.
- Minimal error handling/logging (good for learning, not production).

## Ideas for next steps

- Add a graceful shutdown for active clients.
- Add per-connection timeouts.
- Support IPv6.
- Add a simple text protocol (commands like `TIME`, `ECHO`, `QUIT`).
