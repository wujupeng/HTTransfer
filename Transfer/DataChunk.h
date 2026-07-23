#pragma once

#include <cstdint>
#include <memory>
#include "Core/Common/Types.h"

namespace ht {

struct DataChunk {
    uint64_t chunk_index = 0;
    offset_t offset = 0;
    size_t size = 0;
    std::unique_ptr<uint8_t[]> buffer;

    DataChunk() = default;

    DataChunk(uint64_t idx, offset_t off, size_t sz, std::unique_ptr<uint8_t[]> buf)
        : chunk_index(idx), offset(off), size(sz), buffer(std::move(buf)) {}

    DataChunk(DataChunk&& other) noexcept = default;
    DataChunk& operator=(DataChunk&& other) noexcept = default;

    DataChunk(const DataChunk&) = delete;
    DataChunk& operator=(const DataChunk&) = delete;
};

}