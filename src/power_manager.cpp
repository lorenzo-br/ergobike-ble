#include "power_manager.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#endif

PowerManager::PowerManager()
    : m_lastActivityMs(0)
    , m_sleepPending(false)
{
    memset(&m_config, 0, sizeof(m_config));
}

void PowerManager::begin(const BikeConfig& config) {
    setConfig(config);
#ifndef NATIVE_TEST
    m_lastActivityMs = millis();
    configureWakeupSources();
#endif
}

void PowerManager::setConfig(const BikeConfig& config) {
    m_config = config;
    if (m_config.inactivitySleepSec < 30) {
        m_config.inactivitySleepSec = DEFAULT_INACTIVITY_SLEEP_SEC;
    }
}

void PowerManager::configureWakeupSources() {
#ifndef NATIVE_TEST
    if (PIN_SENSOR <= 5) {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_LOW_LEVEL;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << PIN_SENSOR);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);

        // ESP32-C3 RTC GPIOs (0 to 5) support Deep Sleep (< 15 µA)
        esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_SENSOR, ESP_GPIO_WAKEUP_GPIO_LOW);
    } else {
        // Digital GPIOs (> 5, e.g. GPIO 21) support Light Sleep (~1.5 mA)
        // The sensor pin is already configured by main before this method runs.
        gpio_wakeup_enable((gpio_num_t)PIN_SENSOR, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
    }
#endif
}

void PowerManager::resetInactivityTimer() {
#ifndef NATIVE_TEST
    m_lastActivityMs = millis();
#endif
    m_sleepPending = false;
}

void PowerManager::update(bool isPedaling, bool isBleConnected) {
#ifndef NATIVE_TEST
    uint32_t now = millis();

    // Active state: keep timer fresh
    if (isPedaling || isBleConnected) {
        m_lastActivityMs = now;
        m_sleepPending = false;
        return;
    }

    uint32_t inactiveDurationMs = now - m_lastActivityMs;
    uint32_t sleepThresholdMs = (uint32_t)m_config.inactivitySleepSec * 1000UL;

    if (inactiveDurationMs >= sleepThresholdMs) {
        m_sleepPending = true;
        enterDeepSleep();
    }
#endif
}

void PowerManager::enterDeepSleep() {
#ifndef NATIVE_TEST
    Serial.println("[POWER] Inactivity timeout reached. Entering sleep mode...");
    Serial.flush();

    // Turn off LED before sleep
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, HIGH); // Off for active-low

    if (PIN_SENSOR <= 5) {
        // True Deep Sleep for RTC GPIOs (0 to 5)
        esp_deep_sleep_start();
    } else {
        // Light Sleep for Digital GPIOs (keeps RAM & wakes on pulse)
        esp_light_sleep_start();
        m_lastActivityMs = millis();
        m_sleepPending = false;
        Serial.println("[POWER] Woke up from Light Sleep!");
    }
#endif
}

uint32_t PowerManager::getSecondsUntilSleep() const {
#ifndef NATIVE_TEST
    uint32_t now = millis();
    uint32_t elapsedMs = now - m_lastActivityMs;
    uint32_t thresholdMs = (uint32_t)m_config.inactivitySleepSec * 1000UL;

    if (elapsedMs >= thresholdMs) return 0;
    return (thresholdMs - elapsedMs) / 1000UL;
#else
    return 180;
#endif
}

bool PowerManager::isSleepPending() const {
    return m_sleepPending;
}
