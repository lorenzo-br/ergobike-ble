#include <cassert>
#include <cstddef>
#include <cstdint>

#include "ftms_indoor_bike_data.h"

int main() {
    assert(ftms::estimatePowerWatts(0.0f) == 0);
    assert(ftms::estimatePowerWatts(60.0f) == 126);
    assert(ftms::estimatePowerWatts(90.0f) == 270);
    assert(ftms::estimatePowerWatts(-5.0f) == 0);
    assert(ftms::estimatePowerWatts(200.0f) == 800);

    uint8_t packet[8] = {};
    assert(ftms::buildIndoorBikeData(15.0f, 60.0f, packet, sizeof(packet)) == 8);

    // Flags 0x0044: instantaneous cadence and instantaneous power.
    assert(packet[0] == 0x44);
    assert(packet[1] == 0x00);
    // 15.00 km/h -> 1500 in 0.01 km/h units.
    assert(packet[2] == 0xDC);
    assert(packet[3] == 0x05);
    // 60 RPM -> 120 in 0.5 RPM units.
    assert(packet[4] == 0x78);
    assert(packet[5] == 0x00);
    // Estimated 126 W -> signed 16-bit little-endian.
    assert(packet[6] == 0x7E);
    assert(packet[7] == 0x00);

    assert(ftms::buildIndoorBikeData(15.0f, 60.0f, packet, 7) == 0);

    return 0;
}
