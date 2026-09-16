#pragma once

#include <stdint.h>
#include <stddef.h>

// =============================================================================
// HARDWARE PIN DEFINITIONS (ESP32-C3)
// =============================================================================
#ifndef PIN_SENSOR
#define PIN_SENSOR          21  // GPIO 21: P2 Jack tip (Sensor input, interrupt + wakeup)
#endif

#ifndef PIN_BATTERY_ADC
#define PIN_BATTERY_ADC     0   // GPIO 0: ADC1_CH0 (100k/100k voltage divider to 18650)
#endif

#ifndef PIN_BTN_SETUP
#define PIN_BTN_SETUP       9   // GPIO 9: Native BOOT button (Active LOW, hold for Wi-Fi Setup)
#endif

#ifndef PIN_LED_STATUS
#define PIN_LED_STATUS      8   // GPIO 8: Status LED (Active LOW on most C3 modules)
#endif

#ifndef ERGOBIKE_FTMS_ONLY
#define ERGOBIKE_FTMS_ONLY  0   // Set to 1 for MyWhoosh compatibility testing
#endif

// =============================================================================
// DEFAULT FACTORY PARAMETERS
// =============================================================================
#define DEFAULT_DEVICE_NAME         "ErgoBike-BLE"
#define DEFAULT_WHEEL_CIRC_MM       5000      // Effective distance per sensor pulse (5 m per pedal revolution)
#define DEFAULT_GEAR_RATIO          1.00f     // Bike sensor gives 1 pulse per crank (pedal) revolution
#define DEFAULT_DEBOUNCE_MS         15        // Sensor software debounce filter (milliseconds)
#define DEFAULT_INACTIVITY_SLEEP_SEC 180      // 3 minutes before auto Deep Sleep
#define DEFAULT_STOP_TIMEOUT_MS     2200      // Stop detection: 2.2 seconds with no pulse => 0 RPM / 0 km/h
#define DEFAULT_ADC_CAL_FACTOR      2.00f     // 100k/100k divider multiplier

#define AP_SSID                     "ErgoBike-Setup"
#define AP_PASS                     "12345678"
#define AP_IP_STR                   "192.168.4.1"

// =============================================================================
// BLE SERVICE & CHARACTERISTIC UUIDS (Bluetooth SIG Standard)
// =============================================================================
// Cycling Speed and Cadence Service (CSCS)
#define UUID_SERVICE_CSC            "1816"
#define UUID_CHAR_CSC_MEASUREMENT   "2A5B"
#define UUID_CHAR_CSC_FEATURE       "2A5C"
#define UUID_CHAR_SENSOR_LOCATION   "2A5D"
#define UUID_CHAR_CSC_CONTROL_POINT "2A55"
#define CSC_APPEARANCE              0x0485  // Cycling: Speed and Cadence Sensor
#define CSC_SENSOR_LOCATION         0x05    // Left Crank

// Fitness Machine Service (FTMS)
#define UUID_SERVICE_FTMS           "1826"
#define UUID_CHAR_INDOOR_BIKE_DATA  "2AD2"
#define UUID_CHAR_FTMS_FEATURE      "2ACC"
#define UUID_CHAR_FTMS_CONTROL_POINT "2AD9"
#define UUID_CHAR_FTMS_STATUS       "2ADA"

// Battery Service (BAS)
#define UUID_SERVICE_BATTERY        "180F"
#define UUID_CHAR_BATTERY_LEVEL     "2A19"

// Device Information Service (DIS)
#define UUID_SERVICE_DEVICE_INFO    "180A"
#define UUID_CHAR_MANUFACTURER      "2A29"
#define UUID_CHAR_MODEL_NUMBER      "2A24"
#define UUID_CHAR_FIRMWARE_REV      "2A26"

// =============================================================================
// SYSTEM CONFIGURATION STRUCT (Stored in NVS)
// =============================================================================
struct BikeConfig {
    char deviceName[32];
    uint16_t wheelCircMm;       // Effective distance per sensor pulse, in millimeters
    float gearRatio;            // Flywheel revolutions per 1 pedal crank turn
    uint16_t debounceMs;        // Debounce window in ms
    uint16_t inactivitySleepSec;// Timeout before deep sleep
    float adcMultiplier;        // Calibration factor for voltage divider
    uint16_t nvsVersion;        // Data structure schema version
};

// =============================================================================
// RUNTIME METRICS STRUCT
// =============================================================================
struct BikeMetrics {
    float speedKmh;             // Virtual speed in km/h
    float cadenceRpm;           // Pedal cadence in RPM
    uint32_t totalPulses;       // Total pulses counted
    uint32_t cumulativeWheelRevs;
    uint16_t lastWheelEventTime1024; // 1/1024 s resolution
    uint16_t cumulativeCrankRevs;
    uint16_t lastCrankEventTime1024; // 1/1024 s resolution
    float batteryVoltage;       // Battery Volts
    uint8_t batteryPercentage;  // Battery 0-100%
    bool isConnected;           // BLE client connected
    bool isPedaling;            // Currently moving
};
