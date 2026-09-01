#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

class PowerManager {
public:
    PowerManager();

    void begin(const BikeConfig& config);
    void setConfig(const BikeConfig& config);

    void resetInactivityTimer();
    void update(bool isPedaling, bool isBleConnected);
    void enterDeepSleep();

    uint32_t getSecondsUntilSleep() const;
    bool isSleepPending() const;

private:
    BikeConfig m_config;
    uint32_t m_lastActivityMs;
    bool m_sleepPending;

    void configureWakeupSources();
};
