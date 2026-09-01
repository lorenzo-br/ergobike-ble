# ErgoBike BLE Sensor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a complete, production-ready, ultra-low power ESP32-C3 firmware that converts stationary bike P1/P2 pulse signals into standard BLE Cycling Speed and Cadence (CSCS) and Fitness Machine Service (FTMS) data with a responsive Wi-Fi calibration web portal, battery monitoring, and deep sleep.

**Architecture:** Modular C++ firmware structured around an ISR sensor processor with software debouncing and ring-buffer moving average, dual-stack BLE broadcasting (CSCS 0x1816 + FTMS 0x1826 + Battery 0x180F), NVS persistent configuration storage, an asynchronous captive Wi-Fi portal with a live WebSocket calibration wizard and OTA updater, and an ultra-low power manager with GPIO-wake deep sleep.

**Tech Stack:** C++17, ESP32-C3 Arduino Framework / ESP-IDF, NimBLE-Arduino (or standard ESP32 BLE), ESPAsyncWebServer / WebSockets, Preferences (NVS), PlatformIO build configuration, and native desktop C++ unit testing for core math and signal algorithms.

## Global Constraints

- Target Microcontroller: ESP32-C3 (RISC-V single-core, 4MB Flash).
- Enclosure compatibility: V9 3D enclosure (ESP32 compartment + 18650 battery holder).
- Sensor Pin: `GPIO 3` (Input with internal pull-up and interrupt, wake-capable).
- Battery ADC Pin: `GPIO 0` (ADC1 Channel 0 with 100k/100k voltage divider).
- Setup Button: `GPIO 9` (Native BOOT pin, active LOW).
- Status LED: `GPIO 8` (Active LOW / HIGH configurable).
- BLE Services: CSCS (`0x1816`), FTMS (`0x1826`), Battery (`0x180F`), Device Info (`0x180A`).
- Target Apps: CycleGo, Zwift, Rouvy, Kinomap, Strava, Wahoo.

---

## File Structure Map

- `platformio.ini`: PlatformIO project configuration for ESP32-C3 and native unit tests.
- `include/config.h`: Pin definitions, default constants, BLE UUIDs, and system structures.
- `include/sensor_reader.h` & `src/sensor_reader.cpp`: Hardware interrupt handler, debounce filter, circular buffer, speed/cadence calculations.
- `include/storage_manager.h` & `src/storage_manager.cpp`: NVS flash storage for settings and calibration parameters.
- `include/battery_monitor.h` & `src/battery_monitor.cpp`: ADC readings, voltage calibration, and battery percentage curve.
- `include/ble_manager.h` & `src/ble_manager.cpp`: BLE GATT server, CSCS characteristic formatter (1/1024s timebase), FTMS characteristic formatter, and battery notification.
- `include/power_manager.h` & `src/power_manager.cpp`: Inactivity detection, timer reset, light sleep, and GPIO3 deep sleep wakeup.
- `include/web_portal.h` & `src/web_portal.cpp`: Wi-Fi AP server, live dashboard HTML/JS, 10-pedal calibration wizard, settings REST API, and OTA firmware updater.
- `src/main.cpp`: Main orchestrator, setup boot mode detection (Wi-Fi Config vs BLE Trainer), and main execution loop.
- `test/test_sensor_math.cpp`: Desktop unit tests for debounce, moving average, cadence/speed calculation, and 1/1024s timer rollover.
- `test/test_battery_math.cpp`: Desktop unit tests for ADC voltage divider calculations and battery percentage mapping.

---

### Task 1: Project Environment & PlatformIO Setup

**Files:**
- Create: `platformio.ini`
- Create: `include/config.h`
- Create: `.gitignore`

**Interfaces:**
- Produces: Base configuration types (`BikeConfig`), pin definitions (`PIN_SENSOR`, `PIN_BATTERY_ADC`, `PIN_LED_STATUS`, `PIN_BTN_SETUP`), and UUID definitions (`UUID_SERVICE_CSC`, `UUID_CHAR_CSC_MEAS`, etc.).

- [ ] **Step 1: Create `.gitignore`**
  Add standard PlatformIO, build, and editor ignore entries.

- [ ] **Step 2: Create `platformio.ini`**
  Configure `esp32-c3-devkitm-1` environment with appropriate board flags, upload speed, partitions, and dependencies (`ESP Async WebServer`, `AsyncTCP`, etc.) plus a `native` environment for running unit tests on host PC.

- [ ] **Step 3: Create `include/config.h`**
  Define all pin mappings, BLE UUID constants, default calibration numbers (e.g. `DEFAULT_WHEEL_CIRC_MM = 2096`, `DEFAULT_GEAR_RATIO = 4.5f`, `DEFAULT_DEBOUNCE_MS = 15`), and configuration struct.

- [ ] **Step 4: Verify build configuration syntax**
  Check that configuration headers and platformio setup are valid.

- [ ] **Step 5: Commit**
  `git add platformio.ini include/config.h .gitignore && git commit -m "chore: setup platformio config and project headers"`

---

### Task 2: Core Algorithm Unit Tests & SensorReader Engine

**Files:**
- Create: `include/sensor_reader.h`
- Create: `src/sensor_reader.cpp`
- Create: `test/test_sensor_math.cpp`

**Interfaces:**
- Consumes: `include/config.h`
- Produces: `class SensorReader` with methods:
  - `void begin()`
  - `void handlePulse(uint64_t timestamp_us)`
  - `void update()`
  - `float getCadenceRpm() const`
  - `float getSpeedKmh() const`
  - `uint32_t getCumulativeWheelRevs() const`
  - `uint16_t getLastWheelEventTime1024() const`
  - `uint16_t getCumulativeCrankRevs() const`
  - `uint16_t getLastCrankEventTime1024() const`
  - `void startCalibration()`
  - `float finishCalibration(uint16_t pedal_turns)`
  - `bool isCalibrating() const`
  - `uint32_t getCalibrationPulseCount() const`

- [ ] **Step 1: Write desktop test file for sensor math**
  Write tests in `test/test_sensor_math.cpp` verifying:
  - Debounce filtering (< `debounce_ms` ignored).
  - Accurate RPM & Speed calculation given $\Delta t$ and gear ratio.
  - Zero timeout (stops cadence/speed after 2 seconds of no pulses).
  - Calculation of 10-turn calibration ratio.
  - $1/1024\text{ s}$ resolution timebase and rollover math.

- [ ] **Step 2: Run test to verify it fails (TDD)**
  Execute native test runner (via C++ / g++ / test script) and verify failure before implementation.

- [ ] **Step 3: Implement `include/sensor_reader.h` and `src/sensor_reader.cpp`**
  Implement ISR pulse handling, rolling average buffer, and timeout zeroing logic.

- [ ] **Step 4: Run test to verify it passes**
  Execute native test runner and confirm 100% pass rate.

- [ ] **Step 5: Commit**
  `git add include/sensor_reader.h src/sensor_reader.cpp test/test_sensor_math.cpp && git commit -m "feat: implement sensor reader engine and algorithm unit tests"`

---

### Task 3: Battery Monitoring Engine & Unit Tests

**Files:**
- Create: `include/battery_monitor.h`
- Create: `src/battery_monitor.cpp`
- Create: `test/test_battery_math.cpp`

**Interfaces:**
- Consumes: `include/config.h`
- Produces: `class BatteryMonitor` with methods:
  - `void begin()`
  - `void update()`
  - `float getVoltage()`
  - `uint8_t getPercentage()`
  - `bool isCritical()`

- [ ] **Step 1: Write test for battery voltage and percentage mapping**
  Create `test/test_battery_math.cpp` validating Li-ion discharge curve interpolation:
  - $\ge 4.20\text{V} \rightarrow 100\%$
  - $3.85\text{V} \rightarrow \approx 60\%$
  - $3.70\text{V} \rightarrow \approx 30\%$
  - $3.20\text{V} \rightarrow 0\%$
  - $< 3.00\text{V} \rightarrow \text{critical alert}$.

- [ ] **Step 2: Run test to verify failure**

- [ ] **Step 3: Implement `include/battery_monitor.h` and `src/battery_monitor.cpp`**
  Implement ADC reading with multi-sampling (smoothing) and calibration divider ratio.

- [ ] **Step 4: Run test to verify pass**

- [ ] **Step 5: Commit**
  `git add include/battery_monitor.h src/battery_monitor.cpp test/test_battery_math.cpp && git commit -m "feat: implement battery monitor and discharge curve tests"`

---

### Task 4: NVS Storage Management

**Files:**
- Create: `include/storage_manager.h`
- Create: `src/storage_manager.cpp`

**Interfaces:**
- Consumes: `include/config.h` (`struct BikeConfig`)
- Produces: `class StorageManager` with methods:
  - `bool begin()`
  - `BikeConfig getConfig()`
  - `void saveConfig(const BikeConfig& cfg)`
  - `void resetDefaults()`

- [ ] **Step 1: Define `StorageManager` header and serialization layout**
- [ ] **Step 2: Implement Preferences / NVS storage read/write in `src/storage_manager.cpp`**
- [ ] **Step 3: Verify fallback to defaults when NVS is empty or uninitialized**
- [ ] **Step 4: Commit**
  `git add include/storage_manager.h src/storage_manager.cpp && git commit -m "feat: implement nvs storage manager for user preferences"`

---

### Task 5: BLE Server & GATT Services (CSCS + FTMS + Battery)

**Files:**
- Create: `include/ble_manager.h`
- Create: `src/ble_manager.cpp`

**Interfaces:**
- Consumes: `SensorReader`, `BatteryMonitor`, `config.h`
- Produces: `class BLEManager` with methods:
  - `void begin(const char* deviceName)`
  - `void update(const SensorReader& sensor, const BatteryMonitor& battery)`
  - `bool isConnected() const`
  - `void stop()`

- [ ] **Step 1: Define GATT Services and Characteristics structures**
  - Cycling Speed and Cadence Service (`0x1816`) with `0x2A5B` (Measurement), `0x2A5C` (Feature), `0x2A5D` (Location).
  - Fitness Machine Service (`0x1826`) with `0x2AD2` (Indoor Bike Data), `0x2ACC` (Feature).
  - Battery Service (`0x180F`) with `0x2A19` (Battery Level).
  - Device Information (`0x180A`).
- [ ] **Step 2: Implement BLE packet encoder conforming to Bluetooth SIG specifications**
  Correct little-endian byte ordering, 1/1024s timebase formatting, speed in 0.01 km/h and cadence in 0.5 RPM.
- [ ] **Step 3: Implement connection callbacks and advertising auto-restart**
- [ ] **Step 4: Commit**
  `git add include/ble_manager.h src/ble_manager.cpp && git commit -m "feat: implement dual ble stack cscs ftms and battery service"`

---

### Task 6: Power Management & Deep Sleep Controller

**Files:**
- Create: `include/power_manager.h`
- Create: `src/power_manager.cpp`

**Interfaces:**
- Consumes: `config.h`, `SensorReader`, `BLEManager`
- Produces: `class PowerManager` with methods:
  - `void begin()`
  - `void resetInactivityTimer()`
  - `void update(bool isPedaling, bool isBleConnected)`
  - `void enterDeepSleep()`
  - `uint32_t getSecondsUntilSleep() const`

- [ ] **Step 1: Implement inactivity timer calculation**
- [ ] **Step 2: Configure ESP32-C3 GPIO3 interrupt wake-up (`esp_deep_sleep_enable_gpio_wakeup`)**
- [ ] **Step 3: Implement clean pre-sleep shutdown (NVS flush, radio down)**
- [ ] **Step 4: Commit**
  `git add include/power_manager.h src/power_manager.cpp && git commit -m "feat: implement power manager with gpio wake deep sleep"`

---

### Task 7: Wi-Fi Setup Portal, Live WebSocket Dashboard, Calibration Wizard & OTA

**Files:**
- Create: `include/web_portal.h`
- Create: `src/web_portal.cpp`
- Create: `include/web_assets.h` (Embedded HTML5 / CSS3 / JavaScript single-page application)

**Interfaces:**
- Consumes: `SensorReader`, `BatteryMonitor`, `StorageManager`, `config.h`
- Produces: `class WebPortal` with methods:
  - `void begin(StorageManager* storage, SensorReader* sensor, BatteryMonitor* battery)`
  - `void update()`
  - `void stop()`

- [ ] **Step 1: Build modern, responsive single-page Web App (`web_assets.h`)**
  - **Live Dashboard:** Live RPM gauge, speed gauge, total pulses.
  - **10-Pedal Calibration Wizard:** Step-by-step interactive screen calculating gear ratio automatically.
  - **Settings Panel:** Wheel circumference, debounce time, deep sleep timer, battery voltage calibration.
  - **OTA Firmware Upload:** Form to flash `.bin` directly over Wi-Fi.
- [ ] **Step 2: Implement Async WebServer endpoints and WebSocket broadcaster**
  - `GET /`: Serves dashboard/config UI.
  - `GET /api/status`: JSON with live RPM, speed, battery, pulses, and config.
  - `POST /api/save`: Updates NVS settings.
  - `POST /api/calibrate/start` & `POST /api/calibrate/finish`: Runs wizard calculation.
  - `POST /update`: Handles multipart firmware upload.
- [ ] **Step 3: Implement DNS captive portal (redirects any URL to setup page)**
- [ ] **Step 4: Commit**
  `git add include/web_portal.h src/web_portal.cpp include/web_assets.h && git commit -m "feat: implement wifi setup portal live dashboard calibration wizard and ota"`

---

### Task 8: Main Application Orchestrator & Boot Logic

**Files:**
- Modify: `src/main.cpp`
- Create/Update: `README.md` (Complete wiring diagram, flashing instructions, calibration guide)

**Interfaces:**
- Orchestrates: All modules (`SensorReader`, `BLEManager`, `BatteryMonitor`, `StorageManager`, `PowerManager`, `WebPortal`).

- [ ] **Step 1: Implement `src/main.cpp`**
  - Boot mode detection: check if BOOT button is held $\ge 2\text{s}$ or if wake from deep sleep was requested.
  - If Config Mode: Start Wi-Fi AP + WebPortal, flash LED rapidly.
  - If Normal Mode: Start BLE Advertising + SensorReader + BatteryMonitor + PowerManager, blink LED with heartbeat.
- [ ] **Step 2: Run end-to-end unit tests and verification checks**
- [ ] **Step 3: Document hardware wiring, 3D enclosure assembly, and CycleGo app pairing in `README.md`**
- [ ] **Step 4: Commit**
  `git add src/main.cpp README.md && git commit -m "feat: integrate main orchestrator and documentation"`

---

## Plan Verification & Execution Strategy

1. **Host-based TDD:** Run native algorithmic tests on workstation before flashing.
2. **Deterministic Calibration:** Validate the 10-turn math against known flywheel ratios.
3. **App Verification:** Verify CSCS & FTMS packet integrity and compatibility with CycleGo and Zwift.
