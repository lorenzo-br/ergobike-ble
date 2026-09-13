#include "ble_manager.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <NimBLEDevice.h>

static NimBLEServer* s_pServer = nullptr;
static NimBLECharacteristic* s_pCharCscMeas = nullptr;
static NimBLECharacteristic* s_pCharFtmsData = nullptr;
static NimBLECharacteristic* s_pCharBatteryLevel = nullptr;
static BLEManager* s_instance = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        if (s_instance) {
            s_instance->setDeviceConnected(true);
        }
    }

    void onDisconnect(NimBLEServer* pServer) override {
        if (s_instance) {
            s_instance->setDeviceConnected(false);
        }
        // Auto restart advertising after disconnect
        NimBLEDevice::startAdvertising();
    }
};

static ServerCallbacks s_serverCallbacks;
#endif

BLEManager::BLEManager()
    : m_isConnected(false)
    , m_initialized(false)
    , m_lastNotifyMs(0)
    , m_lastBatteryNotifyMs(0)
    , m_lastBatteryPercentage(255)
{
    memset(m_deviceName, 0, sizeof(m_deviceName));
}

void BLEManager::setDeviceConnected(bool connected) {
    m_isConnected = connected;
}

bool BLEManager::isConnected() const {
    return m_isConnected;
}

void BLEManager::begin(const char* deviceName) {
    if (deviceName && strlen(deviceName) > 0) {
        strncpy(m_deviceName, deviceName, sizeof(m_deviceName) - 1);
    } else {
        strncpy(m_deviceName, DEFAULT_DEVICE_NAME, sizeof(m_deviceName) - 1);
    }

#ifndef NATIVE_TEST
    s_instance = this;
    NimBLEDevice::init(m_deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // +9dBm TX power for maximum range & stability

    s_pServer = NimBLEDevice::createServer();
    s_pServer->setCallbacks(&s_serverCallbacks);

    // =========================================================================
    // 1. Cycling Speed and Cadence Service (CSCS - 0x1816)
    // =========================================================================
    NimBLEService* pCscService = s_pServer->createService(UUID_SERVICE_CSC);

    s_pCharCscMeas = pCscService->createCharacteristic(
        UUID_CHAR_CSC_MEASUREMENT,
        NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic* pCharCscFeature = pCscService->createCharacteristic(
        UUID_CHAR_CSC_FEATURE,
        NIMBLE_PROPERTY::READ
    );
    uint16_t cscFeatures = 0x0003; // Wheel Rev Data + Crank Rev Data supported
    pCharCscFeature->setValue((uint8_t*)&cscFeatures, 2);

    NimBLECharacteristic* pCharSensorLoc = pCscService->createCharacteristic(
        UUID_CHAR_SENSOR_LOCATION,
        NIMBLE_PROPERTY::READ
    );
    uint8_t sensorLocation = 0x0C; // Rear Dropout / Indoor Trainer
    pCharSensorLoc->setValue(&sensorLocation, 1);

    pCscService->start();

    // =========================================================================
    // 2. Fitness Machine Service (FTMS - 0x1826)
    // =========================================================================
    NimBLEService* pFtmsService = s_pServer->createService(UUID_SERVICE_FTMS);

    s_pCharFtmsData = pFtmsService->createCharacteristic(
        UUID_CHAR_INDOOR_BIKE_DATA,
        NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic* pCharFtmsFeature = pFtmsService->createCharacteristic(
        UUID_CHAR_FTMS_FEATURE,
        NIMBLE_PROPERTY::READ
    );
    uint32_t ftmsFeatures[2] = { 0x00000002, 0x00000000 }; // Cadence supported
    pCharFtmsFeature->setValue((uint8_t*)ftmsFeatures, 8);

    pFtmsService->start();

    // =========================================================================
    // 3. Battery Service (BAS - 0x180F)
    // =========================================================================
    NimBLEService* pBatteryService = s_pServer->createService(UUID_SERVICE_BATTERY);

    s_pCharBatteryLevel = pBatteryService->createCharacteristic(
        UUID_CHAR_BATTERY_LEVEL,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    uint8_t initialLevel = 100;
    s_pCharBatteryLevel->setValue(&initialLevel, 1);

    pBatteryService->start();

    // =========================================================================
    // 4. Device Information Service (DIS - 0x180A)
    // =========================================================================
    NimBLEService* pDevInfoService = s_pServer->createService(UUID_SERVICE_DEVICE_INFO);

    NimBLECharacteristic* pCharMfg = pDevInfoService->createCharacteristic(
        UUID_CHAR_MANUFACTURER,
        NIMBLE_PROPERTY::READ
    );
    pCharMfg->setValue("ErgoBike BLE");

    NimBLECharacteristic* pCharModel = pDevInfoService->createCharacteristic(
        UUID_CHAR_MODEL_NUMBER,
        NIMBLE_PROPERTY::READ
    );
    pCharModel->setValue("ESP32-C3 V9");

    NimBLECharacteristic* pCharFw = pDevInfoService->createCharacteristic(
        UUID_CHAR_FIRMWARE_REV,
        NIMBLE_PROPERTY::READ
    );
    pCharFw->setValue("1.0.0");

    pDevInfoService->start();

    // =========================================================================
    // 5. BLE Advertising Configuration
    // =========================================================================
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(UUID_SERVICE_CSC);
    pAdvertising->addServiceUUID(UUID_SERVICE_FTMS);
    pAdvertising->addServiceUUID(UUID_SERVICE_BATTERY);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // Fast connection interval (7.5ms)
    pAdvertising->setMaxPreferred(0x12); // (22.5ms)
    pAdvertising->start();
#endif

    m_initialized = true;
}

void BLEManager::notifyCSC(const SensorReader& sensor) {
#ifndef NATIVE_TEST
    if (!s_pCharCscMeas || !s_pCharCscMeas->getSubscribedCount()) return;

    // CSCS Measurement packet format:
    // Byte 0: Flags (0x03 -> Wheel Revs present + Crank Revs present)
    // Bytes 1..4: Cumulative Wheel Revolutions (uint32_t, little-endian)
    // Bytes 5..6: Last Wheel Event Time (uint16_t, 1/1024s, little-endian)
    // Bytes 7..8: Cumulative Crank Revolutions (uint16_t, little-endian)
    // Bytes 9..10: Last Crank Event Time (uint16_t, 1/1024s, little-endian)
    uint8_t buffer[11];
    buffer[0] = 0x03;

    uint32_t wheelRevs = sensor.getCumulativeWheelRevs();
    buffer[1] = (uint8_t)(wheelRevs & 0xFF);
    buffer[2] = (uint8_t)((wheelRevs >> 8) & 0xFF);
    buffer[3] = (uint8_t)((wheelRevs >> 16) & 0xFF);
    buffer[4] = (uint8_t)((wheelRevs >> 24) & 0xFF);

    uint16_t wheelTime = sensor.getLastWheelEventTime1024();
    buffer[5] = (uint8_t)(wheelTime & 0xFF);
    buffer[6] = (uint8_t)((wheelTime >> 8) & 0xFF);

    uint16_t crankRevs = sensor.getCumulativeCrankRevs();
    buffer[7] = (uint8_t)(crankRevs & 0xFF);
    buffer[8] = (uint8_t)((crankRevs >> 8) & 0xFF);

    uint16_t crankTime = sensor.getLastCrankEventTime1024();
    buffer[9] = (uint8_t)(crankTime & 0xFF);
    buffer[10] = (uint8_t)((crankTime >> 8) & 0xFF);

    s_pCharCscMeas->setValue(buffer, sizeof(buffer));
    s_pCharCscMeas->notify();
#endif
}

void BLEManager::notifyFTMS(const SensorReader& sensor) {
#ifndef NATIVE_TEST
    if (!s_pCharFtmsData || !s_pCharFtmsData->getSubscribedCount()) return;

    // FTMS Indoor Bike Data packet format:
    // Bytes 0..1: Flags (0x0002 -> Instantaneous Cadence Present;
    // bit 0 = 0 keeps Instantaneous Speed present)
    // Bytes 2..3: Instantaneous Speed (uint16_t, unit: 0.01 km/h)
    // Bytes 4..5: Instantaneous Cadence (uint16_t, unit: 0.5 RPM)
    uint8_t buffer[6];
    uint16_t flags = 0x0002;
    buffer[0] = (uint8_t)(flags & 0xFF);
    buffer[1] = (uint8_t)((flags >> 8) & 0xFF);

    uint16_t speedUnits = (uint16_t)(sensor.getSpeedKmh() * 100.0f);
    buffer[2] = (uint8_t)(speedUnits & 0xFF);
    buffer[3] = (uint8_t)((speedUnits >> 8) & 0xFF);

    uint16_t cadenceUnits = (uint16_t)(sensor.getCadenceRpm() * 2.0f);
    buffer[4] = (uint8_t)(cadenceUnits & 0xFF);
    buffer[5] = (uint8_t)((cadenceUnits >> 8) & 0xFF);

    s_pCharFtmsData->setValue(buffer, sizeof(buffer));
    s_pCharFtmsData->notify();
#endif
}

void BLEManager::notifyBattery(uint8_t percentage) {
#ifndef NATIVE_TEST
    if (!s_pCharBatteryLevel) return;
    s_pCharBatteryLevel->setValue(&percentage, 1);
    if (s_pCharBatteryLevel->getSubscribedCount()) {
        s_pCharBatteryLevel->notify();
    }
#endif
}

void BLEManager::update(const SensorReader& sensor, const BatteryMonitor& battery) {
#ifndef NATIVE_TEST
    uint32_t now = millis();

    // Broadcast speed & cadence updates every 500ms (or on demand)
    if (now - m_lastNotifyMs >= 500) {
        m_lastNotifyMs = now;
        notifyCSC(sensor);
        notifyFTMS(sensor);
    }

    // Broadcast battery level every 30 seconds (or if percentage changed)
    uint8_t currentPct = battery.getPercentage();
    if (currentPct != m_lastBatteryPercentage || (now - m_lastBatteryNotifyMs >= 30000)) {
        m_lastBatteryNotifyMs = now;
        m_lastBatteryPercentage = currentPct;
        notifyBattery(currentPct);
    }
#endif
}

void BLEManager::stop() {
#ifndef NATIVE_TEST
    if (NimBLEDevice::getAdvertising()) {
        NimBLEDevice::getAdvertising()->stop();
    }
    NimBLEDevice::deinit(true);
#endif
    m_isConnected = false;
    m_initialized = false;
}
