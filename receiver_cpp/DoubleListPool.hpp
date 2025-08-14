#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <condition_variable>

class DoubleListPool {
public:
    static constexpr std::size_t kPayload = 100;

    struct Node {
        std::array<std::uint8_t, kPayload> data{};
        Node* next = nullptr;
    };

    // capacity_hint: how many nodes to preallocate (e.g., 1024)
    explicit DoubleListPool(std::size_t capacity_hint = 0);

    ~DoubleListPool();
    // ----- producer-side API -----

    // Get an empty node to fill. Allocates a new one if the free list is empty.
    Node* getFree() {
        std::lock_guard<std::mutex> lk(mx_);
        if (closed_) return nullptr;
        Node* n = try_pop_free_unsafe();
        if (!n) {
            n = new Node(); // expand pool on demand
        } else {
            --free_count_;
        }
        return n;
    }

    // After filling node->data, push to the tail of the ready list and wake a consumer.
    bool addNode(Node* n) {
        if (!n) return false;
        std::lock_guard<std::mutex> lk(mx_);
        if (closed_) { delete n; return false; }
        n->next = nullptr;
        if (!ready_tail_) {
            ready_head_ = ready_tail_ = n;
        } else {
            ready_tail_->next = n;
            ready_tail_ = n;
        }
        ++ready_count_;
        cv_not_empty_.notify_one();
        return true;
    }

    // ----- consumer-side API -----

    // Blocking: pop one ready node; returns nullptr if closed and empty.
    Node* getNode() {
        std::unique_lock<std::mutex> lk(mx_);
        cv_not_empty_.wait(lk, [&]{ return closed_ || (ready_head_ != nullptr); });
        if (!ready_head_) return nullptr; // closed & drained
        Node* n = try_pop_ready_unsafe();
        --ready_count_;
        // If we just made room, wake potential producer waiting for "space" (not used here, but ok)
        cv_not_full_.notify_one();
        return n;
    }

    // After processing/printing, return the node to the free list.
    void addFree(Node* n) {
        if (!n) return;
        std::lock_guard<std::mutex> lk(mx_);
        n->next = free_head_;
        free_head_ = n;
        ++free_count_;
        cv_not_full_.notify_one();
    }

    // Non-blocking peek sizes (approximate)
    std::size_t readySize() const {
        std::lock_guard<std::mutex> lk(mx_);
        return ready_count_;
    }
    std::size_t freeSize() const {
        std::lock_guard<std::mutex> lk(mx_);
        return free_count_;
    }

    // Signal shutdown: wake any waiters; future getFree/addNode fail.
    void close() {
        std::lock_guard<std::mutex> lk(mx_);
        closed_ = true;
        cv_not_empty_.notify_all();
        cv_not_full_.notify_all();
    }

private:
    // Unsafe helpers (caller holds mx_)
    Node* try_pop_free_unsafe() {
        Node* n = free_head_;
        if (n) free_head_ = n->next;
        return n;
    }
    Node* try_pop_ready_unsafe() {
        Node* n = ready_head_;
        if (!n) return nullptr;
        ready_head_ = n->next;
        if (!ready_head_) ready_tail_ = nullptr;
        n->next = nullptr;
        return n;
    }

private:
    // lists
    Node* free_head_;
    std::size_t free_count_;
    Node* ready_head_;
    Node* ready_tail_;
    std::size_t ready_count_;

    // sync
    mutable std::mutex mx_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_; // present for symmetry/future capacity logic
    bool closed_;
};
