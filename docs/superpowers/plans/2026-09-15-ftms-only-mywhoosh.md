# MyWhoosh FTMS-Only Compatibility Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide a reversible ESP32-C3 firmware environment that exposes only FTMS for MyWhoosh compatibility testing while preserving the existing dual CSC + FTMS firmware.

**Architecture:** Keep the current dual-stack behavior as the default. Add a compile-time `ERGOBIKE_FTMS_ONLY` profile that conditionally omits the CSC service, CSC notifications, and CSC advertising UUID; FTMS and Battery remain unchanged. Add a dedicated PlatformIO environment so the test image can be flashed without changing the default production profile.

**Tech Stack:** C++17, Arduino ESP32-C3, NimBLE-Arduino, PlatformIO, native C++ tests.

**Spec:** `docs/superpowers/specs/2026-08-31-ergobike-ble-sensor-design.md`

## Global Constraints

- Target board: `esp32-c3-devkitm-1`.
- Sensor input: `GPIO21`.
- FTMS service: `0x1826`, Indoor Bike Data characteristic `0x2AD2`.
- FTMS instantaneous speed unit: `0.01 km/h`.
- FTMS instantaneous cadence unit: `0.5 RPM`.
- The default `esp32-c3` environment remains dual CSC + FTMS.
- The FTMS-only image is a reversible compatibility test; restore the default environment if MyWhoosh does not accept it.
- No USB and bike connection is required at the same time: flash with the sensor disconnected, then test from the battery.

---

### Task 1: Add a tested FTMS-only profile switch

**Files:**
- Modify: `include/config.h:25-35`
- Modify: `include/ble_manager.h:9-32`
- Create: `test/test_ble_profile.cpp`

**Interfaces:**
- Consumes: optional compiler macro `ERGOBIKE_FTMS_ONLY`.
- Produces: `BLEManager::cscServiceEnabled()` as a compile-time profile query for tests and build-time behavior.

- [ ] **Step 1: Write the failing test**

Create `test/test_ble_profile.cpp`:

```cpp
#include <cassert>
#include "ble_manager.h"

int main() {
    assert(BLEManager::cscServiceEnabled() == false);
    return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run from `H:\ergobike-ble`:

```powershell
g++ -std=c++17 -DNATIVE_TEST -DERGOBIKE_FTMS_ONLY=1 -Iinclude test/test_ble_profile.cpp -o test_ble_profile.exe
```

Expected: compilation fails because `BLEManager::cscServiceEnabled()` does not exist yet.

- [ ] **Step 3: Add the profile macro and query**

Add to `include/config.h`:

```cpp
#ifndef ERGOBIKE_FTMS_ONLY
#define ERGOBIKE_FTMS_ONLY 0
#endif
```

Add to `BLEManager` in `include/ble_manager.h`:

```cpp
static constexpr bool cscServiceEnabled() {
    return ERGOBIKE_FTMS_ONLY == 0;
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run:

```powershell
g++ -std=c++17 -DNATIVE_TEST -DERGOBIKE_FTMS_ONLY=1 -Iinclude test/test_ble_profile.cpp -o test_ble_profile.exe
test_ble_profile.exe
```

Expected: exit code `0`.

- [ ] **Step 5: Remove the generated test executable**

```powershell
Remove-Item -LiteralPath test_ble_profile.exe -ErrorAction SilentlyContinue
```

- [ ] **Step 6: Commit the tested profile switch**

```powershell
git add include/config.h include/ble_manager.h test/test_ble_profile.cpp
git commit -m "test: add FTMS-only BLE profile switch"
```

### Task 2: Implement conditional BLE service exposure

**Files:**
- Modify: `src/ble_manager.cpp:66-90`
- Modify: `src/ble_manager.cpp:153-160`
- Modify: `src/ble_manager.cpp:166-200`
- Modify: `src/ble_manager.cpp:238-247`

**Interfaces:**
- Consumes: `ERGOBIKE_FTMS_ONLY` and `BLEManager::cscServiceEnabled()` from Task 1.
- Produces: FTMS-only builds with service UUID `0x1826`, Indoor Bike Data `0x2AD2`, and Battery service `0x180F`; dual builds retain CSC behavior.

- [ ] **Step 1: Write the failing build check**

Run the FTMS-only environment before conditional guards exist:

```powershell
pio run -e esp32-c3-ftms-only
```

Expected: fail because the environment does not exist yet.

- [ ] **Step 2: Guard CSC creation and advertising**

Wrap the CSC service creation, feature, sensor location, and `pCscService->start()` block in `begin()` with:

```cpp
#if !ERGOBIKE_FTMS_ONLY
    // existing CSC service setup
#endif
```

Wrap the CSC advertising line similarly:

```cpp
#if !ERGOBIKE_FTMS_ONLY
    pAdvertising->addServiceUUID(UUID_SERVICE_CSC);
#endif
```

- [ ] **Step 3: Guard CSC notification paths**

Wrap the body of `notifyCSC()` and its call in `update()` with `#if !ERGOBIKE_FTMS_ONLY`. FTMS notifications must continue to run every 500 ms in both profiles.

- [ ] **Step 4: Add the dedicated PlatformIO environment**

Append to `platformio.ini`:

```ini
[env:esp32-c3-ftms-only]
extends = env:esp32-c3
build_flags =
    ${env:esp32-c3.build_flags}
    -D ERGOBIKE_FTMS_ONLY=1
```

- [ ] **Step 5: Run the FTMS-only build**

```powershell
pio run -e esp32-c3-ftms-only
```

Expected: successful ESP32-C3 build with no CSC compilation errors.

- [ ] **Step 6: Run the default dual-stack build**

```powershell
pio run -e esp32-c3
```

Expected: successful build; default profile remains unchanged.

- [ ] **Step 7: Run all native tests**

```powershell
pio test -e native
```

Expected: existing sensor and battery suites pass.

- [ ] **Step 8: Commit the conditional BLE implementation**

```powershell
git add src/ble_manager.cpp platformio.ini
git commit -m "feat: add FTMS-only MyWhoosh test environment"
```

### Task 3: Flash and validate the FTMS-only image

**Files:**
- Build artifact: `.pio/build/esp32-c3-ftms-only/firmware.bin`
- No source changes.

**Interfaces:**
- Consumes: `esp32-c3-ftms-only` environment from Task 2.
- Produces: flashed image that advertises FTMS and Battery but not CSC.

- [ ] **Step 1: Disconnect the sensor cable and keep USB connected**

Leave the bike sensor/Y cable disconnected during flashing so the ESP32-C3 can be powered and monitored through USB.

- [ ] **Step 2: Flash FTMS-only**

```powershell
pio run -e esp32-c3-ftms-only -t upload --upload-port COM5
```

Expected: `Hash of data verified` and `SUCCESS`.

- [ ] **Step 3: Reconnect sensor and battery after flashing**

Disconnect USB, reconnect the LM393 output to `GPIO21`, connect the battery, and do not hold BOOT.

- [ ] **Step 4: Verify with nRF Connect**

Expected:
- `Fitness Machine (0x1826)` is present.
- `Indoor Bike Data (0x2AD2)` has notifications.
- Instantaneous Speed and Instantaneous Cadence change while pedaling.
- CSC service `0x1816` is absent.

- [ ] **Step 5: Verify with MyWhoosh**

Close nRF Connect fully, connect `ErgoBike-BLE` in MyWhoosh as the indoor bike/smart trainer, and pedal. Record whether RPM changes and record the displayed speed in both `km/h` and `m/s`.

- [ ] **Step 6: Decide compatibility result**

If RPM works, keep FTMS-only for MyWhoosh and calibrate speed from the FTMS value. If RPM remains zero, restore the default dual-stack image and investigate MyWhoosh's CSC cadence slot or app limitations.
