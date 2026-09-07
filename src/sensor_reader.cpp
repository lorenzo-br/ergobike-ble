#include "sensor_reader.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <esp_timer.h>
#endif

SensorReader::SensorReader()
    : m_lastPulseTimeUs(0)
    , m_lastProcessedTimeUs(0)
    , m_totalPulses(0)
    , m_cumulativeWheelRevs(0)
    , m_lastWheelEventTime1024(0)
    , m_cumulativeCrankRevs(0)
    , m_lastCrankEventTime1024(0)
    , m_crankAccumulator(0.0f)
    , m_speedKmh(0.0f)
    , m_cadenceRpm(0.0f)
    , m_isPedaling(false)
    , m_bufferIndex(0)
    , m_bufferCount(0)
    , m_isCalibrating(false)
    , m_calibPulseCount(0)
{
    memset(&m_config, 0, sizeof(m_config));
    resetBuffer();
}

void SensorReader::resetBuffer() {
    m_bufferIndex = 0;
    m_bufferCount = 0;
    for (uint8_t i = 0; i < SENSOR_RING_BUFFER_SIZE; ++i) {
        m_deltaBuffer[i] = 0;
    }
}

void SensorReader::begin(const BikeConfig& config) {
    setConfig(config);
    resetBuffer();
    m_lastPulseTimeUs = 0;
    m_totalPulses = 0;
    m_cumulativeWheelRevs = 0;
    m_cumulativeCrankRevs = 0;
    m_crankAccumulator = 0.0f;
    m_speedKmh = 0.0f;
    m_cadenceRpm = 0.0f;
    m_isPedaling = false;
    m_isCalibrating = false;
    m_calibPulseCount = 0;
}

void SensorReader::setConfig(const BikeConfig& config) {
    m_config = config;
    if (m_config.gearRatio <= 0.01f) {
        m_config.gearRatio = DEFAULT_GEAR_RATIO;
    }
    if (m_config.wheelCircMm == 0) {
        m_config.wheelCircMm = DEFAULT_WHEEL_CIRC_MM;
    }
    if (m_config.debounceMs == 0) {
        m_config.debounceMs = DEFAULT_DEBOUNCE_MS;
    }
}

uint16_t SensorReader::timeUsTo1024(uint64_t time_us) {
    // 1 second = 1024 ticks (1/1024 s resolution, 16-bit rollover ~64 seconds)
    return (uint16_t)(((time_us * 1024ULL) / 1000000ULL) & 0xFFFF);
}

void SensorReader::onPulse(uint64_t timestamp_us) {
    if (m_lastPulseTimeUs != 0) {
        uint64_t elapsed_us = timestamp_us - m_lastPulseTimeUs;
        uint64_t debounce_us = (uint64_t)m_config.debounceMs * 1000ULL;
        
        // Discard mechanical bounce / contact jitter
        if (elapsed_us < debounce_us) {
            return;
        }

        // Add interval to smoothing circular buffer
        m_deltaBuffer[m_bufferIndex] = elapsed_us;
        m_bufferIndex = (m_bufferIndex + 1) % SENSOR_RING_BUFFER_SIZE;
        if (m_bufferCount < SENSOR_RING_BUFFER_SIZE) {
            m_bufferCount++;
        }

        // Calculate moving average delta
        uint64_t sum = 0;
        for (uint8_t i = 0; i < m_bufferCount; ++i) {
            sum += m_deltaBuffer[i];
        }
        uint64_t avg_delta_us = sum / m_bufferCount;

        if (avg_delta_us > 0) {
            // Cadence (RPM) = 60 / crank_time_s
            // Crank time (s) = time between flywheel pulses * gearRatio
            float crank_time_s = ((float)avg_delta_us * m_config.gearRatio) / 1000000.0f;
            if (crank_time_s > 0.0001f) {
                m_cadenceRpm = 60.0f / crank_time_s;
                
                // Speed (km/h) = (Circumference (m) / Time for 1 wheel rev (s)) * 3.6
                // Each pulse is 1 wheel/flywheel revolution (consistent with CSCS cumulativeWheelRevs)
                float wheel_circ_m = (float)m_config.wheelCircMm / 1000.0f;
                float wheel_time_s = (float)avg_delta_us / 1000000.0f;
                m_speedKmh = (wheel_circ_m / wheel_time_s) * 3.6f;
                m_isPedaling = true;
            }
        }
    }

    m_lastPulseTimeUs = timestamp_us;
    m_totalPulses++;
    m_cumulativeWheelRevs++;
    m_lastWheelEventTime1024 = timeUsTo1024(timestamp_us);

    // Track fractional crank revolutions
    if (m_config.gearRatio > 0.01f) {
        m_crankAccumulator += 1.0f / m_config.gearRatio;
        if (m_crankAccumulator >= 1.0f) {
            uint16_t fullTurns = (uint16_t)m_crankAccumulator;
            m_cumulativeCrankRevs += fullTurns;
            m_crankAccumulator -= (float)fullTurns;
            m_lastCrankEventTime1024 = m_lastWheelEventTime1024;
        }
    }

    if (m_isCalibrating) {
        m_calibPulseCount++;
    }
}

void SensorReader::update(uint64_t current_time_us) {
    if (m_isPedaling && m_lastPulseTimeUs != 0) {
        uint64_t elapsed_us = current_time_us - m_lastPulseTimeUs;
        uint64_t timeout_us = (uint64_t)DEFAULT_STOP_TIMEOUT_MS * 1000ULL;
        
        if (elapsed_us > timeout_us) {
            // No pulse detected within timeout => stopped
            m_speedKmh = 0.0f;
            m_cadenceRpm = 0.0f;
            m_isPedaling = false;
            resetBuffer();
        }
    }
}

float SensorReader::getSpeedKmh() const { return m_speedKmh; }
float SensorReader::getCadenceRpm() const { return m_cadenceRpm; }
uint32_t SensorReader::getTotalPulses() const { return m_totalPulses; }
uint32_t SensorReader::getCumulativeWheelRevs() const { return m_cumulativeWheelRevs; }
uint16_t SensorReader::getLastWheelEventTime1024() const { return m_lastWheelEventTime1024; }
uint16_t SensorReader::getCumulativeCrankRevs() const { return m_cumulativeCrankRevs; }
uint16_t SensorReader::getLastCrankEventTime1024() const { return m_lastCrankEventTime1024; }
bool SensorReader::isPedaling() const { return m_isPedaling; }

void SensorReader::startCalibration() {
    m_calibPulseCount = 0;
    m_isCalibrating = true;
}

float SensorReader::finishCalibration(uint16_t pedalTurns) {
    m_isCalibrating = false;
    if (pedalTurns > 0 && m_calibPulseCount > 0) {
        float calculatedRatio = (float)m_calibPulseCount / (float)pedalTurns;
        m_config.gearRatio = calculatedRatio;
        return calculatedRatio;
    }
    return m_config.gearRatio;
}

bool SensorReader::isCalibrating() const { return m_isCalibrating; }
uint32_t SensorReader::getCalibrationPulseCount() const { return m_calibPulseCount; }
