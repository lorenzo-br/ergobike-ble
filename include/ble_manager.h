#pragma once

#include "config.h"
#include "sensor_reader.h"
#include "battery_monitor.h"
#include <stdint.h>
#include <stdbool.h>

class BLEManager {
public:
    BLEManager();

    void begin(const char* deviceName);
    void update(const SensorReader& sensor, const BatteryMonitor& battery);
    bool isConnected() const;
    void stop();

    void setDeviceConnected(bool connected);

    static constexpr bool cscServiceEnabled() {
        return ERGOBIKE_FTMS_ONLY == 0;
    }

private:
    char m_deviceName[32];
    bool m_isConnected;
    bool m_initialized;

    uint32_t m_lastNotifyMs;
    uint32_t m_lastBatteryNotifyMs;
    uint8_t m_lastBatteryPercentage;

    void notifyCSC(const SensorReader& sensor);
    void notifyFTMS(const SensorReader& sensor);
    void notifyBattery(uint8_t percentage);
};
