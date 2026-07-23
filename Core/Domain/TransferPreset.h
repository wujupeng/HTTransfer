#pragma once

#include "Config/ConfigManager.h"

namespace ht {

inline PresetConfig getPresetDefault(TransferPreset preset) {
    switch (preset) {
        case TransferPreset::Fast: return {TransferPreset::Fast, 16, false, false, 0};
        case TransferPreset::Secure: return {TransferPreset::Secure, 8, true, true, 100};
        case TransferPreset::Balanced: return {TransferPreset::Balanced, 16, true, true, 0};
    }
    return {TransferPreset::Balanced, 16, true, true, 0};
}

}