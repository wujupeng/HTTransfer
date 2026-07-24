#include "Blake3Calculator.h"
#include <blake3.h>
#include <fstream>
#include <vector>
#include "Core/Common/Types.h"

namespace ht {

struct Blake3Calculator::Context {
    blake3_hasher hasher;
};

Blake3Calculator::Blake3Calculator() : ctx_(std::make_unique<Context>()) {
    blake3_hasher_init(&ctx_->hasher);
}

Blake3Calculator::~Blake3Calculator() = default;

void Blake3Calculator::update(const uint8_t* data, size_t length) {
    blake3_hasher_update(&ctx_->hasher, data, length);
}

std::string Blake3Calculator::finalize() {
    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&ctx_->hasher, output, BLAKE3_OUT_LEN);

    std::string result;
    result.reserve(BLAKE3_OUT_LEN * 2);
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < BLAKE3_OUT_LEN; ++i) {
        result += hex[output[i] >> 4];
        result += hex[output[i] & 0x0F];
    }

    blake3_hasher_init(&ctx_->hasher);
    return result;
}

std::string Blake3Calculator::compute(const uint8_t* data, size_t length) {
    Blake3Calculator calc;
    calc.update(data, length);
    return calc.finalize();
}

std::string Blake3Calculator::computeFile(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) return "";

    Blake3Calculator calc;
    std::vector<uint8_t> buffer(1024 * 1024);
    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        auto count = static_cast<size_t>(file.gcount());
        if (count > 0) {
            calc.update(buffer.data(), count);
        }
    }
    return calc.finalize();
}

}