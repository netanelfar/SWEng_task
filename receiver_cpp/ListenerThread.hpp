#pragma once
#include <atomic>
#include <thread>
#include <string>
#include <array>
#include <cstddef>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "DoubleListPool.hpp" // pool with Node{ std::array<uint8_t,100> data; }

class ListenerThread {
public:
    explicit ListenerThread(unsigned short port, DoubleListPool& pool);
    ~ListenerThread();

    // Start background thread: init Winsock, bind+listen. Returns false on immediate failure.
    bool start();

    // Stop (idempotent): signal thread to exit, close sockets to unblock, join.
    void stop();

private:
    void threadMain();
    bool initWinsock();
    void cleanupWinsock();
    bool bindAndListen();
    bool acceptOne();
    bool recvAll(void* buf, std::size_t len);

private:
    unsigned short      port_;
    DoubleListPool&     pool_;

    std::atomic<bool>   running_{false};
    std::thread         th_;

    // Winsock state
    bool                wsaInit_{false};
    SOCKET              listen_{INVALID_SOCKET};
    SOCKET              client_{INVALID_SOCKET};
};
