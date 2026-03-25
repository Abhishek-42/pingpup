#pragma once
#include <vector>
#include <atomic>
#include <cstring>

// Lock-free single-producer single-consumer ring buffer for PCM int16 samples
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : buf_(capacity), capacity_(capacity), head_(0), tail_(0) {}

    // Returns number of samples written (may be less than count if near full)
    size_t write(const int16_t* data, size_t count) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_acquire);
        size_t avail = capacity_ - 1 - ((head - tail + capacity_) % capacity_);
        if (count > avail) count = avail;

        for (size_t i = 0; i < count; ++i)
            buf_[(head + i) % capacity_] = data[i];

        head_.store((head + count) % capacity_, std::memory_order_release);
        return count;
    }

    // Returns number of samples read
    size_t read(int16_t* out, size_t count) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t head = head_.load(std::memory_order_acquire);
        size_t avail = (head - tail + capacity_) % capacity_;
        if (count > avail) count = avail;

        for (size_t i = 0; i < count; ++i)
            out[i] = buf_[(tail + i) % capacity_];

        tail_.store((tail + count) % capacity_, std::memory_order_release);
        return count;
    }

    size_t available() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail + capacity_) % capacity_;
    }

    void clear() {
        tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
    }

private:
    std::vector<int16_t> buf_;
    size_t               capacity_;
    std::atomic<size_t>  head_;
    std::atomic<size_t>  tail_;
};
