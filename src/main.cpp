#include <Arduino.h>
#include <esp_timer.h>

#include "config.h"
#include "sensor_reader.h"
#include "battery_monitor.h"
#include "storage_manager.h"
#include "ble_manager.h"
#include "power_manager.h"
#include "web_portal.h"

// System instances
static StorageManager s_storage;
static SensorReader   s_sensor;
static BatteryMonitor s_battery;
static BLEManager     s_ble;
static PowerManager   s_power;
static WebPortal      s_portal;

// Runtime state
static bool s_isConfigMode = false;
static uint32_t s_lastLedToggleMs = 0;
static bool s_ledState = false;
static uint32_t s_lastSerialLogMs = 0;
static uint32_t s_pulseFlashUntilMs = 0;

// Hardware Interrupt for Sensor Pulses
void IRAM_ATTR isr_sensor_trigger() {
    uint64_t now_us = esp_timer_get_time();
    s_sensor.onPulse(now_us);
}

// Check if BOOT button is held at power-on to enter Wi-Fi Setup Mode
bool checkSetupButtonHeld() {
    pinMode(PIN_BTN_SETUP, INPUT_PULLUP);
    if (digitalRead(PIN_BTN_SETUP) == LOW) {
        Serial.println("[BOOT] Setup button pressed. Checking hold duration...");
        uint32_t startMs = millis();
        while (digitalRead(PIN_BTN_SETUP) == LOW) {
            if (millis() - startMs > 2000) {
                return true; // Held for > 2 seconds
            }
            delay(10);
        }
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=========================================");
    Serial.println("   ErgoBike BLE Smart Trainer (ESP32-C3)  ");
    Serial.println("=========================================");

    // Initialize LED
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, HIGH); // Off initially (Active-LOW)

    // Load configuration from NVS
    s_storage.begin();
    BikeConfig config = s_storage.getConfig();

    Serial.printf("[CONFIG] Device Name: %s\n", config.deviceName);
    Serial.printf("[CONFIG] Wheel Circ:  %u mm\n", config.wheelCircMm);
    Serial.printf("[CONFIG] Gear Ratio:  %.2f:1\n", config.gearRatio);
    Serial.printf("[CONFIG] Debounce:    %u ms\n", config.debounceMs);
    Serial.printf("[CONFIG] Sleep Time:  %u sec\n", config.inactivitySleepSec);

    // Initialize Core Modules
    s_sensor.begin(config);
    s_battery.begin(config);
    s_power.begin(config);

    // Check if user requested Wi-Fi Setup Portal
    s_isConfigMode = checkSetupButtonHeld();

    // Attach Sensor Interrupt (GPIO 3)
    pinMode(PIN_SENSOR, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), isr_sensor_trigger, FALLING);

    if (s_isConfigMode) {
        Serial.println("[MODE] >>> INICIANDO MODO WI-FI / PORTAL DE CONFIGURAÇÃO <<<");
        s_portal.begin(&s_storage, &s_sensor, &s_battery);
    } else {
        Serial.println("[MODE] >>> INICIANDO MODO NORMAL / BLUETOOTH LOW ENERGY <<<");
        s_ble.begin(config.deviceName);
        Serial.println("[BLE] Anunciando CSCS (0x1816) + FTMS (0x1826) + Bateria (0x180F)...");
        Serial.println("[BLE] Pronto para conectar com CycleGo, Zwift e outros apps!");
    }
}

void handleLedFeedback() {
    uint32_t now = millis();

    // Flash LED on active pulse
    if (now < s_pulseFlashUntilMs) {
        digitalWrite(PIN_LED_STATUS, LOW); // ON
        return;
    }

    if (s_isConfigMode) {
        // Wi-Fi Config Mode: Fast blink (100ms on / 100ms off)
        if (now - s_lastLedToggleMs >= 100) {
            s_lastLedToggleMs = now;
            s_ledState = !s_ledState;
            digitalWrite(PIN_LED_STATUS, s_ledState ? LOW : HIGH);
        }
    } else {
        if (s_ble.isConnected()) {
            // Connected to App: Solid ON
            digitalWrite(PIN_LED_STATUS, LOW);
        } else {
            // Disconnected / Advertising: Slow pulse every 2s (100ms flash)
            uint32_t cycle = now % 2000;
            digitalWrite(PIN_LED_STATUS, (cycle < 100) ? LOW : HIGH);
        }
    }
}

void loop() {
    uint64_t now_us = esp_timer_get_time();
    uint32_t now_ms = millis();

    // 1. Process Sensor timeouts
    s_sensor.update(now_us);

    // 2. Process LED status
    handleLedFeedback();

    if (s_isConfigMode) {
        // Run Wi-Fi Portal
        s_portal.update();
        s_battery.update();
    } else {
        // Run BLE Normal Mode
        s_battery.update();
        s_ble.update(s_sensor, s_battery);
        s_power.update(s_sensor.isPedaling(), s_ble.isConnected());

        // Periodic Serial Monitor output
        if (now_ms - s_lastSerialLogMs >= 1000) {
            s_lastSerialLogMs = now_ms;
            Serial.printf("[TRAINER] Cad: %5.1f RPM | Spd: %5.1f km/h | Pulses: %6u | Bat: %3u%% (%.2fV) | BLE: %s | SleepIn: %us\n",
                s_sensor.getCadenceRpm(),
                s_sensor.getSpeedKmh(),
                s_sensor.getTotalPulses(),
                s_battery.getPercentage(),
                s_battery.getVoltage(),
                s_ble.isConnected() ? "CONNECTED" : "ADVERTISING",
                s_power.getSecondsUntilSleep()
            );
        }
    }

    delay(5);
}
