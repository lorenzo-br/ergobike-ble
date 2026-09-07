#include "battery_monitor.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <esp_adc_cal.h>
#endif

BatteryMonitor::BatteryMonitor()
    : m_voltage(4.20f)
    , m_percentage(100)
    , m_isCritical(false)
    , m_lastReadMs(0)
{
    memset(&m_config, 0, sizeof(m_config));
}

void BatteryMonitor::begin(const BikeConfig& config) {
    setConfig(config);
#ifndef NATIVE_TEST
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
#endif
    update();
}

void BatteryMonitor::setConfig(const BikeConfig& config) {
    m_config = config;
    if (m_config.adcMultiplier <= 0.1f) {
        m_config.adcMultiplier = DEFAULT_ADC_CAL_FACTOR;
    }
}

float BatteryMonitor::rawAdcToVoltage(uint16_t rawAdc, float multiplier) {
    // 12-bit ADC: 0..4095 mapped to 0..3.3V (with 11dB attenuation, linear range up to ~2.6V)
    float pinVoltage = ((float)rawAdc / 4095.0f) * 3.3f;
    return pinVoltage * multiplier;
}

uint8_t BatteryMonitor::voltageToPercentage(float voltage) {
    if (voltage >= 4.20f) return 100;
    if (voltage <= 3.00f) return 0;

    // Piecewise linear interpolation for standard 18650 Li-ion discharge curve
    struct VoltagePoint {
        float v;
        uint8_t p;
    };

    static const VoltagePoint curve[] = {
        { 4.20f, 100 },
        { 4.10f, 90 },
        { 4.00f, 80 },
        { 3.90f, 70 },
        { 3.80f, 55 },
        { 3.70f, 35 },
        { 3.60f, 20 },
        { 3.50f, 10 },
        { 3.30f, 3 },
        { 3.00f, 0 }
    };
    static const size_t numPoints = sizeof(curve) / sizeof(curve[0]);

    for (size_t i = 0; i < numPoints - 1; ++i) {
        if (voltage <= curve[i].v && voltage >= curve[i + 1].v) {
            float vRange = curve[i].v - curve[i + 1].v;
            float pRange = (float)(curve[i].p - curve[i + 1].p);
            float vFraction = (voltage - curve[i + 1].v) / vRange;
            return (uint8_t)(curve[i + 1].p + (vFraction * pRange));
        }
    }

    return 0;
}

float BatteryMonitor::readAdcVoltage() {
#ifndef NATIVE_TEST
    uint32_t adcSum = 0;
    const uint8_t samples = 16;
    for (uint8_t i = 0; i < samples; ++i) {
        adcSum += analogRead(PIN_BATTERY_ADC);
        delayMicroseconds(50);
    }
    uint16_t avgAdc = adcSum / samples;
    return rawAdcToVoltage(avgAdc, m_config.adcMultiplier);
#else
    return 4.00f; // Default simulated voltage for desktop test
#endif
}

void BatteryMonitor::update() {
#ifndef NATIVE_TEST
    uint32_t now = millis();
    if (m_lastReadMs != 0 && (now - m_lastReadMs) < 5000) {
        return; // Read every 5 seconds to reduce ADC active time
    }
    m_lastReadMs = now;
#endif

    m_voltage = readAdcVoltage();
    if (m_voltage < 2.0f) {
        // Direct power via expansion board battery port (no external divider on GPIO 0)
        m_percentage = 100;
        m_isCritical = false;
    } else {
        m_percentage = voltageToPercentage(m_voltage);
        m_isCritical = (m_voltage < 3.05f);
    }
}

float BatteryMonitor::getVoltage() const { return m_voltage; }
uint8_t BatteryMonitor::getPercentage() const { return m_percentage; }
bool BatteryMonitor::isCritical() const { return m_isCritical; }
bool BatteryMonitor::isMonitored() const { return m_voltage >= 2.0f; }
