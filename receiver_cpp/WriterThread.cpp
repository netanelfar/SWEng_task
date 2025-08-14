#include "WriterThread.hpp"

WriterThread::WriterThread(DoubleListPool& pool, std::string outPath)
    : pool_(pool), outPath_(std::move(outPath)) {}

WriterThread::~WriterThread() { stop(); }

bool WriterThread::start() {
    if (running_.exchange(true)) return true;

    fout_ = std::fopen(outPath_.c_str(), "ab");
    if (!fout_) {
        std::perror("[writer] fopen");
        running_.store(false);
        return false;
    }

    // Give stdio a large buffer to minimize syscalls and blocking on disk
    if (stdio_buf_kb_) {
        std::size_t sz = stdio_buf_kb_ * 1024;
        // static so lifetime outlives setvbuf usage; fine here since one writer per process
        static std::vector<char> buf; 
        buf.resize(sz);
        std::setvbuf(fout_, buf.data(), _IOFBF, buf.size());
    }

    th_ = std::thread(&WriterThread::threadMain, this);
    return true;
}

void WriterThread::stop() {
    if (!running_.exchange(false)) return;
    // No direct signal to pool here — ListenerThread/pool controls closure.
    if (th_.joinable()) th_.join();
    if (fout_) {
        std::fflush(fout_);
        std::fclose(fout_);
        fout_ = nullptr;
    }
}
void WriterThread::wait() {
    if (th_.joinable()) th_.join();
}
void WriterThread::threadMain() {
    while (running_.load()) {
        // Block until there is a ready node or pool is closed and drained.
        DoubleListPool::Node* n = pool_.getNode();
        if (!n) break;  // pool closed + empty => we're done

        // Append 100B
        size_t w = std::fwrite(n->data.data(), 1, n->data.size(), fout_);
        if (w != n->data.size()) {
            std::perror("[writer] fwrite");
            pool_.addFree(n); // return node before exiting
            break;
        }

        // Print a compact line (throttled to avoid console overhead)
        if ((++count_ % 100) == 0) {
            auto &d = n->data;
            std::ios::fmtflags f(std::cout.flags());
            std::cout << "pkt#" << (count_-1) << " first4 "
                      << std::hex << std::uppercase << std::setfill('0')
                      << std::setw(2) << (int)d[0] << " "
                      << std::setw(2) << (int)d[1] << " "
                      << std::setw(2) << (int)d[2] << " "
                      << std::setw(2) << (int)d[3]
                      << std::dec << std::nouppercase << "\n";
            std::cout.flags(f);
        }

        if ((count_ % flush_every_) == 0) {
            std::fflush(fout_);
        }

        // Recycle node to free list
        pool_.addFree(n);
    }
}
