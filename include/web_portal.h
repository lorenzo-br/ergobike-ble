#pragma once

#include "config.h"
#include "storage_manager.h"
#include "sensor_reader.h"
#include "battery_monitor.h"
#include <stdbool.h>

class WebPortal {
public:
    WebPortal();

    void begin(StorageManager* storage, SensorReader* sensor, BatteryMonitor* battery);
    void update();
    void stop();

    bool isRunning() const;

private:
    StorageManager* m_storage;
    SensorReader* m_sensor;
    BatteryMonitor* m_battery;
    bool m_running;

    void setupRoutes();
};
