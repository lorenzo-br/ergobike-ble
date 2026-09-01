#include <cassert>
#include <iostream>
#include <cmath>
#include "sensor_reader.h"

void test_debounce() {
    std::cout << "[TEST] Running test_debounce..." << std::endl;
    SensorReader sensor;
    BikeConfig config = {};
    config.wheelCircMm = 2096;
    config.gearRatio = 4.0f;
    config.debounceMs = 15; // 15ms debounce
    sensor.begin(config);

    uint64_t t = 1000000; // 1.000000 s
    sensor.onPulse(t);
    assert(sensor.getTotalPulses() == 1);

    // Spurious bounce at +5ms (5000us) -> should be ignored
    sensor.onPulse(t + 5000);
    assert(sensor.getTotalPulses() == 1);

    // Valid next pulse at +50ms (50000us) -> accepted
    sensor.onPulse(t + 50000);
    assert(sensor.getTotalPulses() == 2);
    std::cout << "  -> test_debounce PASSED!" << std::endl;
}

void test_speed_cadence_math() {
    std::cout << "[TEST] Running test_speed_cadence_math..." << std::endl;
    SensorReader sensor;
    BikeConfig config = {};
    config.wheelCircMm = 2000; // 2.0 meters circumference
    config.gearRatio = 4.0f;   // 4 flywheel pulses per pedal crank turn
    config.debounceMs = 10;
    sensor.begin(config);

    // Simulate steady pedaling at 60 RPM:
    // 60 RPM = 1 crank turn per second (1000 ms per crank turn)
    // Flywheel turns 4 times per second => 250 ms (250,000 us) per flywheel pulse!
    uint64_t t = 10000000; // 10s
    for (int i = 0; i < 8; ++i) {
        sensor.onPulse(t);
        t += 250000; // +250ms
    }

    float cadence = sensor.getCadenceRpm();
    float speed = sensor.getSpeedKmh();

    std::cout << "  Cadence: " << cadence << " RPM (Expected ~60.0)" << std::endl;
    std::cout << "  Speed: " << speed << " km/h (Expected ~7.2)" << std::endl;

    assert(std::fabs(cadence - 60.0f) < 0.5f);
    assert(std::fabs(speed - 7.2f) < 0.2f);
    assert(sensor.isPedaling() == true);
    std::cout << "  -> test_speed_cadence_math PASSED!" << std::endl;
}

void test_stop_detection() {
    std::cout << "[TEST] Running test_stop_detection..." << std::endl;
    SensorReader sensor;
    BikeConfig config = {};
    config.wheelCircMm = 2096;
    config.gearRatio = 4.5f;
    config.debounceMs = 15;
    sensor.begin(config);

    uint64_t t = 1000000;
    sensor.onPulse(t);
    sensor.onPulse(t + 200000);
    sensor.onPulse(t + 400000);
    assert(sensor.isPedaling() == true);

    // After 1 second, still under 2.2s timeout
    sensor.update(t + 400000 + 1000000);
    assert(sensor.isPedaling() == true);

    // After 2.5 seconds without pulse => stopped
    sensor.update(t + 400000 + 2500000);
    assert(sensor.isPedaling() == false);
    assert(sensor.getSpeedKmh() == 0.0f);
    assert(sensor.getCadenceRpm() == 0.0f);
    std::cout << "  -> test_stop_detection PASSED!" << std::endl;
}

void test_calibration_wizard() {
    std::cout << "[TEST] Running test_calibration_wizard..." << std::endl;
    SensorReader sensor;
    BikeConfig config = {};
    config.gearRatio = 1.0f;
    sensor.begin(config);

    sensor.startCalibration();
    assert(sensor.isCalibrating() == true);

    uint64_t t = 1000000;
    // Simulate 45 pulses during 10 pedal turns
    for (int i = 0; i < 45; ++i) {
        sensor.onPulse(t);
        t += 100000;
    }

    assert(sensor.getCalibrationPulseCount() == 45);
    float calculatedRatio = sensor.finishCalibration(10);
    assert(sensor.isCalibrating() == false);
    assert(std::fabs(calculatedRatio - 4.5f) < 0.01f);
    std::cout << "  Calculated gear ratio: " << calculatedRatio << " (Expected 4.50)" << std::endl;
    std::cout << "  -> test_calibration_wizard PASSED!" << std::endl;
}

void test_timebase_1024() {
    std::cout << "[TEST] Running test_timebase_1024..." << std::endl;
    // 1 second (1,000,000 us) should be 1024 ticks
    uint16_t ticks1s = SensorReader::timeUsTo1024(1000000ULL);
    assert(ticks1s == 1024);

    // 0.5 seconds (500,000 us) should be 512 ticks
    uint16_t ticksHalf = SensorReader::timeUsTo1024(500000ULL);
    assert(ticksHalf == 512);

    // 64 seconds should wrap 16-bit uint16 (64 * 1024 = 65536 -> 0)
    uint16_t wrapTicks = SensorReader::timeUsTo1024(64000000ULL);
    assert(wrapTicks == 0);
    std::cout << "  -> test_timebase_1024 PASSED!" << std::endl;
}

int main() {
    std::cout << "=== RUNNING SENSOR MATH UNIT TESTS ===" << std::endl;
    test_debounce();
    test_speed_cadence_math();
    test_stop_detection();
    test_calibration_wizard();
    test_timebase_1024();
    std::cout << "=== ALL SENSOR MATH TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    return 0;
}
