#include <cassert>
#include <iostream>
#include <cmath>
#include "battery_monitor.h"

void test_voltage_to_percentage() {
    std::cout << "[TEST] Running test_voltage_to_percentage..." << std::endl;

    assert(BatteryMonitor::voltageToPercentage(4.25f) == 100);
    assert(BatteryMonitor::voltageToPercentage(4.20f) == 100);
    assert(BatteryMonitor::voltageToPercentage(4.10f) == 90);
    assert(BatteryMonitor::voltageToPercentage(4.00f) == 80);
    assert(BatteryMonitor::voltageToPercentage(3.90f) == 70);
    assert(BatteryMonitor::voltageToPercentage(3.80f) == 55);
    assert(BatteryMonitor::voltageToPercentage(3.70f) == 35);
    assert(BatteryMonitor::voltageToPercentage(3.60f) == 20);
    assert(BatteryMonitor::voltageToPercentage(3.50f) == 10);
    assert(BatteryMonitor::voltageToPercentage(3.30f) == 3);
    assert(BatteryMonitor::voltageToPercentage(3.00f) == 0);
    assert(BatteryMonitor::voltageToPercentage(2.80f) == 0);

    // Intermediate point: 3.75V should be between 35% and 55% (~45%)
    uint8_t mid = BatteryMonitor::voltageToPercentage(3.75f);
    std::cout << "  3.75V mapped to " << (int)mid << "% (Expected 45%)" << std::endl;
    assert(mid >= 43 && mid <= 47);

    std::cout << "  -> test_voltage_to_percentage PASSED!" << std::endl;
}

void test_raw_adc_conversion() {
    std::cout << "[TEST] Running test_raw_adc_conversion..." << std::endl;

    // 12-bit ADC (4095) with multiplier 2.0:
    // If ADC pin sees 2.10V (rawAdc = 2606), 18650 voltage = 4.20V
    uint16_t rawAdc42 = (uint16_t)((2.10f / 3.3f) * 4095.0f);
    float v42 = BatteryMonitor::rawAdcToVoltage(rawAdc42, 2.0f);
    std::cout << "  Raw ADC " << rawAdc42 << " -> " << v42 << "V (Expected ~4.20V)" << std::endl;
    assert(std::fabs(v42 - 4.20f) < 0.05f);

    // If ADC pin sees 1.85V (rawAdc = 2296), 18650 voltage = 3.70V
    uint16_t rawAdc37 = (uint16_t)((1.85f / 3.3f) * 4095.0f);
    float v37 = BatteryMonitor::rawAdcToVoltage(rawAdc37, 2.0f);
    std::cout << "  Raw ADC " << rawAdc37 << " -> " << v37 << "V (Expected ~3.70V)" << std::endl;
    assert(std::fabs(v37 - 3.70f) < 0.05f);

    std::cout << "  -> test_raw_adc_conversion PASSED!" << std::endl;
}

int main() {
    std::cout << "=== RUNNING BATTERY MATH UNIT TESTS ===" << std::endl;
    test_voltage_to_percentage();
    test_raw_adc_conversion();
    std::cout << "=== ALL BATTERY MATH TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    return 0;
}
