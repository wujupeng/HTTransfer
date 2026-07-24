#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

namespace ht {

class SHA256Calculator {
public:
    SHA256Calculator();
    ~SHA256Calculator();

    SHA256Calculator(const SHA256Calculator&) = delete;
    SHA256Calculator& operator=(const SHA256Calculator&) = delete;

    void update(const uint8_t* data, size_t length);
    std::string finalize();

    static std::string compute(const uint8_t* data, size_t length);

private:
    struct Context;
    std::unique_ptr<Context> ctx_;
};

}