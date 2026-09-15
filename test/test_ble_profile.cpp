#include <cassert>

#include "ble_manager.h"

int main() {
    assert(BLEManager::cscServiceEnabled() == false);
    return 0;
}
