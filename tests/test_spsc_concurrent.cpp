#include "spsc_queue.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <cstdint>

TEST(SPSCConcurrent, ProducerConsumer) {
    const uint64_t count = 1000000;
    SPSCQueue<uint64_t, 1024> q;

    std::thread producer([&] {
        for (uint64_t i = 0; i < count; ++i)
            while (!q.push(i)) { /* busy-wait: consumer will free space */ }
    });

    // consumer thread is a test thread itself
    uint64_t expected = 0, out = 0;
    while (expected < count) {
        if (q.pop(out)) {
            ASSERT_EQ(out, expected);
            ++expected;
        }
    }

    producer.join();
    EXPECT_EQ(expected, count);
}
