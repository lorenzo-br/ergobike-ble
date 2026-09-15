#include <cassert>

#include "storage_manager.h"

int main() {
    StorageManager storage;
    const BikeConfig config = storage.getConfig();

    // One sensor pulse represents one pedal revolution and 5 m of effective travel.
    assert(config.wheelCircMm == 5000);
    assert(config.gearRatio == 1.00f);
    assert(storage::currentNvsVersion == 4);
    assert(storage::migrateWheelCircMm(4367) == 5000);
    assert(storage::migrateWheelCircMm(3000) == 3000);

    return 0;
}
