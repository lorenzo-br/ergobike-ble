#include "ble_manager.h"
#include "csc_control_point.h"
#include "ftms_control_point.h"
#include "ftms_indoor_bike_data.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <NimBLEDevice.h>

static NimBLEServer* s_pServer = nullptr;
static NimBLECharacteristic* s_pCharCscMeas = nullptr;
static NimBLECharacteristic* s_pCharCscControlPoint = nullptr;
static NimBLECharacteristic* s_pCharFtmsData = nullptr;
static NimBLECharacteristic* s_pCharFtmsStatus = nullptr;
static NimBLECharacteristic* s_pCharBatteryLevel = nullptr;
static BLEManager* s_instance = nullptr;
static const SensorReader* s_pSensor = nullptr;
static csc::ControlPointState s_cscControlState;
static int32_t s_cscWheelRevOffset = 0;
static uint32_t s_cscLastRawWheelRevs = 0;
static bool s_cscFirstPacketLogged = false;
static ftms::ControlPointState s_ftmsControlState;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        s_cscControlState = {};
        s_cscWheelRevOffset = 0;
        s_cscLastRawWheelRevs = 0;
        s_cscFirstPacketLogged = false;
        s_ftmsControlState = {};
        if (s_instance) {
            s_instance->setDeviceConnected(true);
        }
    }

    void onDisconnect(NimBLEServer* pServer) override {
        s_cscControlState = {};
        s_cscWheelRevOffset = 0;
        s_cscLastRawWheelRevs = 0;
        s_cscFirstPacketLogged = false;
        s_ftmsControlState = {};
        if (s_instance) {
            s_instance->setDeviceConnected(false);
        }
        // Auto restart advertising after disconnect
        NimBLEDevice::startAdvertising();
    }
};

static ServerCallbacks s_serverCallbacks;

class CscMeasurementCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic* pCharacteristic,
                     ble_gap_conn_desc* desc,
                     uint16_t subValue) override {
        s_cscFirstPacketLogged = false;
        Serial.printf("[CSC] 0x2A5B notifications %s (subValue=0x%04X)\n",
                      (subValue & 0x0001) ? "ENABLED" : "DISABLED",
                      subValue);
    }
};

static CscMeasurementCallbacks s_cscMeasurementCallbacks;

class CscControlPointCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        NimBLEAttValue request = pCharacteristic->getValue();
        uint8_t response[3];
        std::size_t responseLength = csc::buildResponse(
            request.data(), request.size(), response, sizeof(response), s_cscControlState);
        if (responseLength == 0) return;

        Serial.printf("[CSC] Control Point opcode: 0x%02X, status: 0x%02X\n",
                      request.data()[0], response[2]);

        if (request.data()[0] == 0x01 && response[2] == csc::kSuccess) {
            const uint32_t currentRawRevs = s_pSensor
                ? s_pSensor->getCumulativeWheelRevs() : s_cscLastRawWheelRevs;
            s_cscLastRawWheelRevs = currentRawRevs;
            s_cscWheelRevOffset = static_cast<int32_t>(
                s_cscControlState.cumulativeWheelRevolutions - currentRawRevs);
        }

        pCharacteristic->setValue(response, responseLength);
        pCharacteristic->indicate();
    }
};

static CscControlPointCallbacks s_cscControlPointCallbacks;

static void notifyFtmsStatus(uint8_t status) {
    if (!s_pCharFtmsStatus || !s_pCharFtmsStatus->getSubscribedCount()) return;

    s_pCharFtmsStatus->setValue(&status, 1);
    s_pCharFtmsStatus->notify();
}

class FtmsControlPointCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        NimBLEAttValue request = pCharacteristic->getValue();
        uint8_t response[3];
        std::size_t responseLength = ftms::buildResponse(
            request.data(), request.size(), response, sizeof(response), s_ftmsControlState);
        if (responseLength == 0) return;

        Serial.printf("[FTMS] Control Point opcode: 0x%02X, status: 0x%02X\n",
                      request.data()[0], response[2]);
        pCharacteristic->setValue(response, responseLength);
        pCharacteristic->indicate();

        if (response[2] == ftms::kSuccess) {
            switch (request.data()[0]) {
                case 0x01: // Reset
                    notifyFtmsStatus(ftms::kStatusReset);
                    break;
                case 0x07: // Start/Resume
                    notifyFtmsStatus(ftms::kStatusStartedOrResumedByUser);
                    break;
                case 0x08: // Stop/Pause
                    notifyFtmsStatus(ftms::kStatusStoppedOrPausedByUser);
                    break;
                default:
                    break;
            }
        }
    }
};

static FtmsControlPointCallbacks s_ftmsControlPointCallbacks;
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

#if !ERGOBIKE_FTMS_ONLY
    // =========================================================================
    // 1. Cycling Speed and Cadence Service (CSCS - 0x1816)
    // =========================================================================
    NimBLEService* pCscService = s_pServer->createService(UUID_SERVICE_CSC);

    s_pCharCscMeas = pCscService->createCharacteristic(
        UUID_CHAR_CSC_MEASUREMENT,
        NIMBLE_PROPERTY::NOTIFY
    );
    s_pCharCscMeas->setCallbacks(&s_cscMeasurementCallbacks);

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
    uint8_t sensorLocation = CSC_SENSOR_LOCATION;
    pCharSensorLoc->setValue(&sensorLocation, 1);

    s_pCharCscControlPoint = pCscService->createCharacteristic(
        UUID_CHAR_CSC_CONTROL_POINT,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE
    );
    s_pCharCscControlPoint->setCallbacks(&s_cscControlPointCallbacks);

    pCscService->start();
#endif

    // =========================================================================
    // 2. Fitness Machine Service (FTMS - 0x1826)
    // =========================================================================
    NimBLEService* pFtmsService = s_pServer->createService(UUID_SERVICE_FTMS);

    s_pCharFtmsData = pFtmsService->createCharacteristic(
        UUID_CHAR_INDOOR_BIKE_DATA,
        NIMBLE_PROPERTY::NOTIFY
    );

    s_pCharFtmsStatus = pFtmsService->createCharacteristic(
        UUID_CHAR_FTMS_STATUS,
        NIMBLE_PROPERTY::NOTIFY
    );
    uint8_t initialFtmsStatus = ftms::kStatusReset;
    s_pCharFtmsStatus->setValue(&initialFtmsStatus, 1);

    NimBLECharacteristic* pCharFtmsFeature = pFtmsService->createCharacteristic(
        UUID_CHAR_FTMS_FEATURE,
        NIMBLE_PROPERTY::READ
    );
    uint32_t ftmsFeatures[2] = {
        ftms::kMachineFeatureFlags, // Cadence + Power Measurement supported
        0x00000000
    };
    pCharFtmsFeature->setValue((uint8_t*)ftmsFeatures, 8);

    NimBLECharacteristic* pFtmsControlPoint = pFtmsService->createCharacteristic(
        UUID_CHAR_FTMS_CONTROL_POINT,
        NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::INDICATE
    );
    pFtmsControlPoint->setCallbacks(&s_ftmsControlPointCallbacks);

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
#if !ERGOBIKE_FTMS_ONLY
    pAdvertising->setAppearance(CSC_APPEARANCE);
    pAdvertising->addServiceUUID(UUID_SERVICE_CSC);
#endif
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
#if !ERGOBIKE_FTMS_ONLY
    const uint32_t rawWheelRevs = sensor.getCumulativeWheelRevs();
    s_cscLastRawWheelRevs = rawWheelRevs;
    if (!s_pCharCscMeas || !s_pCharCscMeas->getSubscribedCount()) return;

    // CSCS Measurement packet format:
    // Byte 0: Flags (0x03 -> Wheel Revs present + Crank Revs present)
    // Bytes 1..4: Cumulative Wheel Revolutions (uint32_t, little-endian)
    // Bytes 5..6: Last Wheel Event Time (uint16_t, 1/1024s, little-endian)
    // Bytes 7..8: Cumulative Crank Revolutions (uint16_t, little-endian)
    // Bytes 9..10: Last Crank Event Time (uint16_t, 1/1024s, little-endian)
    uint8_t buffer[11];
    buffer[0] = 0x03;

    uint32_t wheelRevs = rawWheelRevs + static_cast<uint32_t>(s_cscWheelRevOffset);
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
    if (!s_cscFirstPacketLogged) {
        Serial.printf("[CSC] 0x2A5B first notify: wheel=%lu, crank=%u, wheelTime=%u, crankTime=%u\n",
                      (unsigned long)wheelRevs,
                      (unsigned)crankRevs,
                      (unsigned)wheelTime,
                      (unsigned)crankTime);
        s_cscFirstPacketLogged = true;
    }
    s_pCharCscMeas->notify();
#else
    (void)sensor;
#endif
#endif
}

void BLEManager::notifyFTMS(const SensorReader& sensor) {
#ifndef NATIVE_TEST
    if (!s_pCharFtmsData || !s_pCharFtmsData->getSubscribedCount()) return;

    // FTMS Indoor Bike Data packet format:
    // Bytes 0..1: Flags (0x0044 -> Instantaneous Cadence + Power Present;
    // bit 2 = Instantaneous Cadence, bit 6 = Instantaneous Power)
    // Bytes 2..3: Instantaneous Speed (uint16_t, unit: 0.01 km/h)
    // Bytes 4..5: Instantaneous Cadence (uint16_t, unit: 0.5 RPM)
    // Bytes 6..7: Instantaneous Power (sint16_t, unit: 1 W; estimated)
    uint8_t buffer[8];
    std::size_t length = ftms::buildIndoorBikeData(
        sensor.getSpeedKmh(), sensor.getCadenceRpm(), buffer, sizeof(buffer));
    if (length == 0) return;

    s_pCharFtmsData->setValue(buffer, length);
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
    s_pSensor = &sensor;
    uint32_t now = millis();

    // Broadcast speed & cadence updates every 500ms (or on demand)
    if (now - m_lastNotifyMs >= 500) {
        m_lastNotifyMs = now;
#if !ERGOBIKE_FTMS_ONLY
        notifyCSC(sensor);
#endif
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
