#include <cstdio>
#include "Core/BufferPool.h"

int main() {
    constexpr uint64_t test_seg_size = 4096;
    ht::BufferPool pool(test_seg_size * 4, test_seg_size);

    auto seg = pool.acquire();
    if (seg.isOk()) {
        pool.release(std::move(seg.value()));
    }

    printf("BufferPool test passed!\n");
    return 0;
}