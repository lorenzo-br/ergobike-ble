#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

class BatteryMonitor {
public:
    BatteryMonitor();

    void begin(const BikeConfig& config);
    void setConfig(const BikeConfig& config);

    // Call periodically in loop (e.g., every 5-10 seconds)
    void update();

    float getVoltage() const;
    uint8_t getPercentage() const;
    bool isCritical() const;

    // Direct math conversion helper for unit tests and calibration
    static uint8_t voltageToPercentage(float voltage);
    static float rawAdcToVoltage(uint16_t rawAdc, float multiplier);

private:
    BikeConfig m_config;
    float m_voltage;
    uint8_t m_percentage;
    bool m_isCritical;
    uint32_t m_lastReadMs;

    float readAdcVoltage();
};
