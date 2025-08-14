#pragma once
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <iomanip>
#include <iostream>

#include "DoubleListPool.hpp"   // Node{ std::array<uint8_t,100> data; }

class WriterThread {
public:
    // outPath: binary file to append packets to (e.g., "packets.bin")
    explicit WriterThread(DoubleListPool& pool, std::string outPath);
    ~WriterThread();

    // Opens file (append, binary), starts background writer loop. Returns false on error.
    bool start();

    // Requests shutdown and joins the thread. Idempotent.
    void stop();
    void wait();
    // Optional tuning (call before start()):
    void setFlushEvery(std::size_t n) { flush_every_ = n ? n : 100; }
    void setStdioBufferKB(std::size_t kb) { stdio_buf_kb_ = kb; }

private:
    void threadMain();

private:
    DoubleListPool&    pool_;
    std::string        outPath_;
    std::FILE*         fout_{nullptr};

    std::atomic<bool>  running_{false};
    std::thread        th_;
    std::size_t        count_{0};       // packets written
    std::size_t        flush_every_{100};
    std::size_t        stdio_buf_kb_{1024}; // 1MB stdio buffer by default
};
