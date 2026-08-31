#pragma once

#include <string>
#include <cstdint>

namespace ht {

struct AppConfig {
    std::string source_path;
    std::string target_path;
    bool multi_thread = true;
    bool overwrite = false;
    bool resume = true;
    bool verify = true;
    bool speed_limit = false;
    int speed_limit_value = 100;
    int thread_count = 4;
    bool auto_start = false;
    std::string language = "en";
    bool minimized_start = false;
};

}