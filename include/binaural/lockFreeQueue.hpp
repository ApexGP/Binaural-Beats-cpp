#pragma once

#include <atomic>
#include <cstddef>
#include <optional>

namespace binaural {

/// 单生产者单消费者 (SPSC) 无锁队列，用于推理线程 → 控制线程传递 EEGStatePrediction
/// 容量固定，满时 push 覆盖最旧；空时 pop 返回 nullopt
template <typename T, size_t Capacity = 8>
class LockFreeQueue {
public:
    LockFreeQueue() : head_(0), tail_(0) {}

    /// 生产者调用：入队，若满则覆盖最旧
    void push(const T& item) {
        size_t w = tail_.load(std::memory_order_relaxed);
        buffer_[w % Capacity] = item;
        tail_.store(w + 1, std::memory_order_release);
    }

    void push(T&& item) {
        size_t w = tail_.load(std::memory_order_relaxed);
        buffer_[w % Capacity] = std::move(item);
        tail_.store(w + 1, std::memory_order_release);
    }

    /// 消费者调用：取最新（丢弃更早的），空则返回 nullopt
    std::optional<T> popLatest() {
        size_t t = tail_.load(std::memory_order_acquire);
        size_t h = head_.load(std::memory_order_relaxed);
        if (h >= t) return std::nullopt;
        // 取最新：跳到最后一个
        head_.store(t, std::memory_order_release);
        return std::move(buffer_[(t - 1) % Capacity]);
    }

    /// 取一个（FIFO），空则返回 nullopt
    std::optional<T> pop() {
        size_t t = tail_.load(std::memory_order_acquire);
        size_t h = head_.load(std::memory_order_relaxed);
        if (h >= t) return std::nullopt;
        T item = std::move(buffer_[h % Capacity]);
        head_.store(h + 1, std::memory_order_release);
        return item;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) >=
               tail_.load(std::memory_order_acquire);
    }

private:
    T buffer_[Capacity];
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};

}  // namespace binaural
