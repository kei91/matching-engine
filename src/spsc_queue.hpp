#pragma once

#include <array>
#include <atomic>

template <typename T, std::size_t N>
class SPSCQueue {
    std::array<T, N> m_buffer;

    alignas(64) std::atomic<std::size_t> read{0};
    alignas(64) std::atomic<std::size_t> write{0};

public:
    bool push(const T& t) {
        std::size_t cur = write.load(std::memory_order_relaxed);
        std::size_t next = (cur + 1) & (N - 1);

        if (next == read.load(std::memory_order_acquire))
            return false;

        m_buffer[cur] = t;
        write.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& t) {
        std::size_t cur = read.load(std::memory_order_relaxed);

        if (cur == write.load(std::memory_order_acquire))
            return false;

        t = m_buffer[cur];

        std::size_t next = (cur + 1) & (N - 1);
        read.store(next, std::memory_order_release);        
        return true;
    }
};