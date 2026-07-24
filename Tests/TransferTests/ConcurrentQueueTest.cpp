#include <gtest/gtest.h>
#include "Transfer/ConcurrentQueue.h"
#include <thread>
#include <vector>
#include <atomic>

namespace ht {

TEST(ConcurrentQueueTest, PushPopBasic) {
    ConcurrentQueue queue(4);
    queue.setActiveReaders(1);

    auto buf = std::make_unique<uint8_t[]>(4);
    memset(buf.get(), 0x01, 4);
    DataChunk chunk_in(0, 0, 4, std::move(buf));

    EXPECT_TRUE(queue.push(std::move(chunk_in)));
    EXPECT_EQ(queue.size(), 1u);

    DataChunk chunk_out;
    EXPECT_TRUE(queue.pop(chunk_out));
    EXPECT_EQ(chunk_out.chunk_index, 0u);
    EXPECT_EQ(chunk_out.size, 4u);
    ASSERT_NE(chunk_out.buffer, nullptr);
    EXPECT_EQ(chunk_out.buffer[0], 0x01);
}

TEST(ConcurrentQueueTest, PopReturnsFalseAfterShutdown) {
    ConcurrentQueue queue(4);
    queue.setActiveReaders(0);
    queue.signalShutdown();

    DataChunk chunk_out;
    EXPECT_FALSE(queue.pop(chunk_out));
}

TEST(ConcurrentQueueTest, PushReturnsFalseAfterWriterError) {
    ConcurrentQueue queue(4);
    queue.signalWriterError();

    auto buf = std::make_unique<uint8_t[]>(4);
    DataChunk chunk_in(0, 0, 4, std::move(buf));
    EXPECT_FALSE(queue.push(std::move(chunk_in)));
}

TEST(ConcurrentQueueTest, PushBlocksWhenFull) {
    ConcurrentQueue queue(2);
    queue.setActiveReaders(1);

    auto buf1 = std::make_unique<uint8_t[]>(1);
    auto buf2 = std::make_unique<uint8_t[]>(1);
    auto buf3 = std::make_unique<uint8_t[]>(1);

    EXPECT_TRUE(queue.push(DataChunk(0, 0, 1, std::move(buf1))));
    EXPECT_TRUE(queue.push(DataChunk(1, 1, 1, std::move(buf2))));

    std::atomic<bool> push_done{false};
    std::thread pusher([&] {
        EXPECT_TRUE(queue.push(DataChunk(2, 2, 1, std::move(buf3))));
        push_done.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(push_done.load());

    DataChunk out;
    EXPECT_TRUE(queue.pop(out));

    pusher.join();
    EXPECT_TRUE(push_done.load());
}

TEST(ConcurrentQueueTest, PopBlocksWhenEmpty) {
    ConcurrentQueue queue(4);
    queue.setActiveReaders(1);

    DataChunk chunk_out;
    std::atomic<bool> pop_done{false};
    std::thread popper([&] {
        DataChunk c;
        queue.pop(c);
        pop_done.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(pop_done.load());

    auto buf = std::make_unique<uint8_t[]>(1);
    queue.push(DataChunk(0, 0, 1, std::move(buf)));

    popper.join();
    EXPECT_TRUE(pop_done.load());
}

TEST(ConcurrentQueueTest, DecrementActiveReadersTriggersShutdown) {
    ConcurrentQueue queue(4);
    queue.setActiveReaders(1);

    auto buf = std::make_unique<uint8_t[]>(1);
    queue.push(DataChunk(0, 0, 1, std::move(buf)));

    DataChunk out;
    EXPECT_TRUE(queue.pop(out));
    EXPECT_EQ(queue.size(), 0u);

    queue.decrementActiveReaders();
    EXPECT_TRUE(queue.isShutdown());

    DataChunk out2;
    EXPECT_FALSE(queue.pop(out2));
}

TEST(ConcurrentQueueTest, DecrementActiveReadersWaitsForQueueToDrain) {
    ConcurrentQueue queue(4);
    queue.setActiveReaders(1);

    auto buf = std::make_unique<uint8_t[]>(1);
    queue.push(DataChunk(0, 0, 1, std::move(buf)));

    queue.decrementActiveReaders();
    EXPECT_FALSE(queue.isShutdown());

    DataChunk out;
    EXPECT_TRUE(queue.pop(out));
}

TEST(ConcurrentQueueTest, MultipleReadersDecrement) {
    ConcurrentQueue queue(4);
    queue.setActiveReaders(3);

    queue.decrementActiveReaders();
    EXPECT_FALSE(queue.isShutdown());

    queue.decrementActiveReaders();
    EXPECT_FALSE(queue.isShutdown());

    queue.decrementActiveReaders();
    EXPECT_TRUE(queue.isShutdown());
}

TEST(ConcurrentQueueTest, ConcurrentPushPop) {
    ConcurrentQueue queue(16);
    queue.setActiveReaders(4);

    const int N = 100;
    std::atomic<int> pushed{0};
    std::atomic<int> popped{0};

    std::vector<std::thread> pushers;
    for (int t = 0; t < 4; ++t) {
        pushers.emplace_back([&, t] {
            for (int i = 0; i < N; ++i) {
                auto buf = std::make_unique<uint8_t[]>(1);
                buf[0] = static_cast<uint8_t>(t);
                if (queue.push(DataChunk(t * N + i, i, 1, std::move(buf)))) {
                    pushed.fetch_add(1);
                }
            }
        });
    }

    std::vector<std::thread> poppers;
    for (int t = 0; t < 2; ++t) {
        poppers.emplace_back([&] {
            DataChunk c;
            while (queue.pop(c)) {
                popped.fetch_add(1);
            }
        });
    }

    for (auto& p : pushers) p.join();

    queue.decrementActiveReaders();
    queue.decrementActiveReaders();
    queue.decrementActiveReaders();
    queue.decrementActiveReaders();

    for (auto& p : poppers) p.join();

    EXPECT_EQ(pushed.load(), popped.load());
    EXPECT_EQ(pushed.load(), 400);
}

TEST(ConcurrentQueueTest, WriterErrorStopsPush) {
    ConcurrentQueue queue(4);
    queue.setActiveReaders(1);

    auto buf = std::make_unique<uint8_t[]>(1);
    EXPECT_TRUE(queue.push(DataChunk(0, 0, 1, std::move(buf))));

    queue.signalWriterError();
    EXPECT_TRUE(queue.isWriterError());

    auto buf2 = std::make_unique<uint8_t[]>(1);
    EXPECT_FALSE(queue.push(DataChunk(1, 1, 1, std::move(buf2))));
}

}