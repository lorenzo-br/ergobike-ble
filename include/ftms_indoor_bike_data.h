#pragma once

#include <cstddef>
#include <cstdint>

namespace ftms {

constexpr uint16_t kIndoorBikeDataFlags = 0x0044;
constexpr uint32_t kMachineFeatureFlags = 0x00004002;

// The bike has no torque sensor, so this is an estimate, not measured power.
inline int16_t estimatePowerWatts(float cadenceRpm) {
    if (!(cadenceRpm > 0.0f)) return 0;

    const float watts = 0.03f * cadenceRpm * cadenceRpm + 0.30f * cadenceRpm;
    if (watts >= 800.0f) return 800;
    return static_cast<int16_t>(watts + 0.5f);
}

// Build Indoor Bike Data with instantaneous speed, cadence, and power.
inline std::size_t buildIndoorBikeData(float speedKmh,
                                       float cadenceRpm,
                                       uint8_t* buffer,
                                       std::size_t capacity) {
    if (!buffer || capacity < 8) return 0;

    if (!(speedKmh > 0.0f)) speedKmh = 0.0f;
    if (!(cadenceRpm > 0.0f)) cadenceRpm = 0.0f;

    float speedUnitsFloat = speedKmh * 100.0f;
    if (speedUnitsFloat > 65535.0f) speedUnitsFloat = 65535.0f;
    const uint16_t speedUnits = static_cast<uint16_t>(speedUnitsFloat + 0.5f);

    float cadenceUnitsFloat = cadenceRpm * 2.0f;
    if (cadenceUnitsFloat > 65535.0f) cadenceUnitsFloat = 65535.0f;
    const uint16_t cadenceUnits = static_cast<uint16_t>(cadenceUnitsFloat + 0.5f);

    const uint16_t powerUnits = static_cast<uint16_t>(estimatePowerWatts(cadenceRpm));

    buffer[0] = static_cast<uint8_t>(kIndoorBikeDataFlags & 0xFF);
    buffer[1] = static_cast<uint8_t>((kIndoorBikeDataFlags >> 8) & 0xFF);
    buffer[2] = static_cast<uint8_t>(speedUnits & 0xFF);
    buffer[3] = static_cast<uint8_t>((speedUnits >> 8) & 0xFF);
    buffer[4] = static_cast<uint8_t>(cadenceUnits & 0xFF);
    buffer[5] = static_cast<uint8_t>((cadenceUnits >> 8) & 0xFF);
    buffer[6] = static_cast<uint8_t>(powerUnits & 0xFF);
    buffer[7] = static_cast<uint8_t>((powerUnits >> 8) & 0xFF);
    return 8;
}

} // namespace ftms
