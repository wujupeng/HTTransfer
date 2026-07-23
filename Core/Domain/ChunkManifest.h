#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "Core/Common/Types.h"
#include "Core/Common/Constants.h"

namespace ht {

enum class ChunkStatus : uint8_t {
    Pending,
    Transferring,
    Verified,
    Failed
};

struct ChunkInfo {
    uint64_t chunk_index = 0;
    uint64_t offset = 0;
    uint64_t size = 0;
    uint32_t crc32 = 0;
    ChunkStatus status = ChunkStatus::Pending;
};

struct ChunkManifest {
    std::string task_id;
    uint64_t chunk_size = kChunkSize;
    uint64_t total_chunks = 0;
    std::string file_hash;
    std::vector<ChunkInfo> chunks;
};

inline ChunkManifest createChunkManifest(const std::string& task_id, uint64_t file_size,
                                          uint64_t chunk_size = kChunkSize) {
    ChunkManifest manifest;
    manifest.task_id = task_id;
    manifest.chunk_size = chunk_size;
    manifest.total_chunks = (file_size + chunk_size - 1) / chunk_size;
    manifest.chunks.reserve(manifest.total_chunks);

    for (uint64_t i = 0; i < manifest.total_chunks; ++i) {
        ChunkInfo chunk;
        chunk.chunk_index = i;
        chunk.offset = i * chunk_size;
        chunk.size = (i == manifest.total_chunks - 1)
                         ? file_size - chunk.offset
                         : chunk_size;
        chunk.status = ChunkStatus::Pending;
        manifest.chunks.push_back(chunk);
    }

    return manifest;
}

}