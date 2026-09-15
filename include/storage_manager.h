#pragma once

#include "config.h"
#include <stdbool.h>

namespace storage {
constexpr uint16_t currentNvsVersion = 4;
constexpr uint16_t legacyDefaultWheelCircMm = 4367;

constexpr uint16_t migrateWheelCircMm(uint16_t storedWheelCircMm) {
    return storedWheelCircMm == legacyDefaultWheelCircMm
        ? DEFAULT_WHEEL_CIRC_MM : storedWheelCircMm;
}
}

class StorageManager {
public:
    StorageManager();

    bool begin();
    BikeConfig getConfig() const;
    void saveConfig(const BikeConfig& cfg);
    void resetDefaults();

private:
    BikeConfig m_config;
    bool m_initialized;

    void loadDefaults();
    void sanitize();
};
