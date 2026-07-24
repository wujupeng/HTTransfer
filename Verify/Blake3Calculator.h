#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <filesystem>

namespace ht {

class Blake3Calculator {
public:
    Blake3Calculator();
    ~Blake3Calculator();

    Blake3Calculator(const Blake3Calculator&) = delete;
    Blake3Calculator& operator=(const Blake3Calculator&) = delete;

    void update(const uint8_t* data, size_t length);
    std::string finalize();

    static std::string compute(const uint8_t* data, size_t length);
    static std::string computeFile(const std::filesystem::path& file_path);

private:
    struct Context;
    std::unique_ptr<Context> ctx_;
};

}