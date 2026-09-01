#pragma once

#include "config.h"
#include <stdbool.h>

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
