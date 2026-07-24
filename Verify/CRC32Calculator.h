#pragma once

#include <cstdint>
#include <cstddef>

namespace ht {

class CRC32Calculator {
public:
    CRC32Calculator();

    void update(const uint8_t* data, size_t length);
    uint32_t finalize();

    static uint32_t compute(const uint8_t* data, size_t length);

private:
    uint32_t crc_;
    static uint32_t table_[256];
    static bool table_initialized_;
    static void initTable();
};

}