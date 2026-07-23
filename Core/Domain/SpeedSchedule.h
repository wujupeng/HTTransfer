#pragma once

#include <string>
#include <cstdint>
#include "Config/ConfigManager.h"

namespace ht {

inline SpeedSchedule makeSpeedSchedule(const std::string& id, const std::string& name,
                                        const std::string& time_start, const std::string& time_end,
                                        uint64_t speed_limit, uint32_t priority = 0, bool enabled = true) {
    return {id, name, time_start, time_end, speed_limit, priority, enabled};
}

}