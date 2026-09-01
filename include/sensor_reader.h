#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#define SENSOR_RING_BUFFER_SIZE 4

class SensorReader {
public:
    SensorReader();

    void begin(const BikeConfig& config);
    void setConfig(const BikeConfig& config);
    
    // Core pulse trigger - can be called from ISR or test harness
    void onPulse(uint64_t timestamp_us);

    // Periodic loop update to detect stops/timeouts
    void update(uint64_t current_time_us);

    // Getters
    float getSpeedKmh() const;
    float getCadenceRpm() const;
    uint32_t getTotalPulses() const;
    uint32_t getCumulativeWheelRevs() const;
    uint16_t getLastWheelEventTime1024() const;
    uint16_t getCumulativeCrankRevs() const;
    uint16_t getLastCrankEventTime1024() const;
    bool isPedaling() const;

    // Calibration Wizard helpers
    void startCalibration();
    float finishCalibration(uint16_t pedalTurns);
    bool isCalibrating() const;
    uint32_t getCalibrationPulseCount() const;

    // Helper conversion from microseconds to 1/1024 seconds
    static uint16_t timeUsTo1024(uint64_t time_us);

private:
    BikeConfig m_config;

    volatile uint64_t m_lastPulseTimeUs;
    uint64_t m_lastProcessedTimeUs;

    uint32_t m_totalPulses;
    uint32_t m_cumulativeWheelRevs;
    uint16_t m_lastWheelEventTime1024;

    uint16_t m_cumulativeCrankRevs;
    uint16_t m_lastCrankEventTime1024;
    float m_crankAccumulator;

    float m_speedKmh;
    float m_cadenceRpm;
    bool m_isPedaling;

    // Moving average filter
    uint64_t m_deltaBuffer[SENSOR_RING_BUFFER_SIZE];
    uint8_t m_bufferIndex;
    uint8_t m_bufferCount;

    // Calibration state
    bool m_isCalibrating;
    uint32_t m_calibPulseCount;

    void resetBuffer();
};
