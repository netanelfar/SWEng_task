#include "ListenerThread.hpp"
#include <iostream>

ListenerThread::ListenerThread(unsigned short port, DoubleListPool& pool)
    : port_(port), pool_(pool) {}

ListenerThread::~ListenerThread() { stop(); }

bool ListenerThread::initWinsock() {
    WSADATA w{};
    if (WSAStartup(MAKEWORD(2,2), &w) != 0) return false;
    wsaInit_ = true;
    return true;
}

void ListenerThread::cleanupWinsock() {
    if (client_ != INVALID_SOCKET) { closesocket(client_); client_ = INVALID_SOCKET; }
    if (listen_ != INVALID_SOCKET) { closesocket(listen_); listen_ = INVALID_SOCKET; }
    if (wsaInit_) { WSACleanup(); wsaInit_ = false; }
}

bool ListenerThread::bindAndListen() {
    listen_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_ == INVALID_SOCKET) { std::cerr << "[listener] socket() failed\n"; return false; }

    int yes = 1;
    setsockopt(listen_, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    sockaddr_in a{};
    a.sin_family      = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port        = htons(port_);

    if (::bind(listen_, (sockaddr*)&a, sizeof a) == SOCKET_ERROR) {
        std::cerr << "[listener] bind failed: " << WSAGetLastError() << "\n";
        return false;
    }
    if (::listen(listen_, 1) == SOCKET_ERROR) {
        std::cerr << "[listener] listen failed: " << WSAGetLastError() << "\n";
        return false;
    }
    std::cout << "[listener] listening on 127.0.0.1:" << port_ << "\n";
    return true;
}

bool ListenerThread::start() {
    if (running_.exchange(true)) return true; // already running

    if (!initWinsock()) { running_.store(false); return false; }
    if (!bindAndListen()){ cleanupWinsock(); running_.store(false); return false; }

    th_ = std::thread(&ListenerThread::threadMain, this);
    return true;
}

void ListenerThread::stop() {
    if (!running_.exchange(false)) return; // already stopped
    // Closing sockets unblocks accept()/recv()
    if (listen_ != INVALID_SOCKET) closesocket(listen_);
    if (client_ != INVALID_SOCKET) closesocket(client_);
    if (th_.joinable()) th_.join();
    cleanupWinsock();
    // Make sure downstream knows we're done
    pool_.close();
}

bool ListenerThread::acceptOne() {
    sockaddr_in cli{}; int clen = sizeof(cli);
    client_ = ::accept(listen_, (sockaddr*)&cli, &clen);
    if (client_ == INVALID_SOCKET) {
        if (running_.load()) {
            std::cerr << "[listener] accept failed: " << WSAGetLastError() << "\n";
        }
        return false;
    }

    // Optional: enlarge socket receive buffer for extra safety
    int rcvbuf = 512 * 1024;
    setsockopt(client_, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf));

    std::cout << "[listener] client connected\n";
    return true;
}

bool ListenerThread::recvAll(void* buf, std::size_t len) {
    char* p = static_cast<char*>(buf);
    std::size_t got = 0;
    while (got < len && running_.load()) {
        int n = ::recv(client_, p + got, (int)(len - got), 0);
        if (n <= 0) return false; // closed or error
        got += (std::size_t)n;
    }
    return got == len;
}

void ListenerThread::threadMain() {
    // Accept exactly one client
    if (!acceptOne()) {
        pool_.close();
        return;
    }

    // Main recv loop: fetch 100B at a time and hand off to pool
    while (running_.load()) {
        // Obtain a free node (allocates if free-list empty)
        DoubleListPool::Node* node = pool_.getFree();
        if (!node) break; // pool closed

        // Fill exactly 100 bytes from TCP stream
        if (!recvAll(node->data.data(), node->data.size())) {
            // socket closed/failure — recycle the node and exit
            pool_.addFree(node);
            break;
        }

        // Hand it to the ready list; if pool closed during handoff, recycle
        if (!pool_.addNode(node)) {
            pool_.addFree(node);
            break;
        }
    }

    // Signal end-of-stream to consumer
    pool_.close();

    // Close client socket on exit
    if (client_ != INVALID_SOCKET) {
        closesocket(client_);
        client_ = INVALID_SOCKET;
    }
}
