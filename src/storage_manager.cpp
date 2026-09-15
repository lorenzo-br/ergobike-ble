#include "storage_manager.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Preferences.h>
static Preferences s_prefs;
#define PREFS_NAMESPACE "ergobike"
#endif

StorageManager::StorageManager()
    : m_initialized(false)
{
    loadDefaults();
}

void StorageManager::loadDefaults() {
    memset(&m_config, 0, sizeof(m_config));
    strncpy(m_config.deviceName, DEFAULT_DEVICE_NAME, sizeof(m_config.deviceName) - 1);
    m_config.wheelCircMm = DEFAULT_WHEEL_CIRC_MM;
    m_config.gearRatio = DEFAULT_GEAR_RATIO;
    m_config.debounceMs = DEFAULT_DEBOUNCE_MS;
    m_config.inactivitySleepSec = DEFAULT_INACTIVITY_SLEEP_SEC;
    m_config.adcMultiplier = DEFAULT_ADC_CAL_FACTOR;
    m_config.nvsVersion = storage::currentNvsVersion;
}

void StorageManager::sanitize() {
    if (strlen(m_config.deviceName) == 0) {
        strncpy(m_config.deviceName, DEFAULT_DEVICE_NAME, sizeof(m_config.deviceName) - 1);
    }
    if (m_config.wheelCircMm < 500 || m_config.wheelCircMm > 5000) {
        m_config.wheelCircMm = DEFAULT_WHEEL_CIRC_MM;
    }
    if (m_config.gearRatio < 0.1f || m_config.gearRatio > 50.0f) {
        m_config.gearRatio = DEFAULT_GEAR_RATIO;
    }
    if (m_config.debounceMs < 2 || m_config.debounceMs > 200) {
        m_config.debounceMs = DEFAULT_DEBOUNCE_MS;
    }
    if (m_config.inactivitySleepSec < 30 || m_config.inactivitySleepSec > 3600) {
        m_config.inactivitySleepSec = DEFAULT_INACTIVITY_SLEEP_SEC;
    }
    if (m_config.adcMultiplier < 0.5f || m_config.adcMultiplier > 10.0f) {
        m_config.adcMultiplier = DEFAULT_ADC_CAL_FACTOR;
    }
}

bool StorageManager::begin() {
#ifndef NATIVE_TEST
    if (!s_prefs.begin(PREFS_NAMESPACE, false)) {
        loadDefaults();
        return false;
    }

    uint16_t ver = s_prefs.getUShort("ver", 0);
    bool needsSave = false;
    if (ver != storage::currentNvsVersion) {
        if (ver == 3) {
            // Migrate v3 -> v4 and replace only the previous shipped default.
            String name = s_prefs.getString("name", DEFAULT_DEVICE_NAME);
            uint16_t circ = s_prefs.getUShort("circ", DEFAULT_WHEEL_CIRC_MM);
            float ratio = s_prefs.getFloat("ratio", DEFAULT_GEAR_RATIO);
            uint16_t deb = s_prefs.getUShort("deb", DEFAULT_DEBOUNCE_MS);
            uint16_t sleep = s_prefs.getUShort("sleep", DEFAULT_INACTIVITY_SLEEP_SEC);
            float adc = s_prefs.getFloat("adc", DEFAULT_ADC_CAL_FACTOR);
            loadDefaults();
            strncpy(m_config.deviceName, name.c_str(), sizeof(m_config.deviceName) - 1);
            m_config.deviceName[sizeof(m_config.deviceName) - 1] = '\0';
            m_config.wheelCircMm = storage::migrateWheelCircMm(circ);
            m_config.gearRatio = ratio;
            m_config.debounceMs = deb;
            m_config.inactivitySleepSec = sleep;
            m_config.adcMultiplier = adc;
            sanitize();
            needsSave = true;
        } else if (ver == 2) {
            // Migrate v2 -> v4: the bike sensor gives 1 pulse per crank turn.
            String name = s_prefs.getString("name", DEFAULT_DEVICE_NAME);
            uint16_t circ = s_prefs.getUShort("circ", DEFAULT_WHEEL_CIRC_MM);
            uint16_t deb = s_prefs.getUShort("deb", DEFAULT_DEBOUNCE_MS);
            uint16_t sleep = s_prefs.getUShort("sleep", DEFAULT_INACTIVITY_SLEEP_SEC);
            float adc = s_prefs.getFloat("adc", DEFAULT_ADC_CAL_FACTOR);
            loadDefaults();
            strncpy(m_config.deviceName, name.c_str(), sizeof(m_config.deviceName) - 1);
            m_config.deviceName[sizeof(m_config.deviceName) - 1] = '\0';
            m_config.wheelCircMm = storage::migrateWheelCircMm(circ);
            m_config.debounceMs = deb;
            m_config.inactivitySleepSec = sleep;
            m_config.adcMultiplier = adc;
            sanitize();
            needsSave = true;
        } else if (ver == 1) {
            // Migrate v1 -> v4: keep the user's calibrated settings, but apply
            // the new virtual circumference so FTMS speed matches the bike computer.
            String name = s_prefs.getString("name", DEFAULT_DEVICE_NAME);
            float ratio = s_prefs.getFloat("ratio", DEFAULT_GEAR_RATIO);
            uint16_t deb = s_prefs.getUShort("deb", DEFAULT_DEBOUNCE_MS);
            uint16_t sleep = s_prefs.getUShort("sleep", DEFAULT_INACTIVITY_SLEEP_SEC);
            float adc = s_prefs.getFloat("adc", DEFAULT_ADC_CAL_FACTOR);
            loadDefaults();
            strncpy(m_config.deviceName, name.c_str(), sizeof(m_config.deviceName) - 1);
            m_config.deviceName[sizeof(m_config.deviceName) - 1] = '\0';
            m_config.gearRatio = ratio;
            m_config.debounceMs = deb;
            m_config.inactivitySleepSec = sleep;
            m_config.adcMultiplier = adc;
            sanitize();
            needsSave = true;
        } else {
            // First boot or unknown schema -> save defaults
            loadDefaults();
            needsSave = true;
        }
    } else {
        String name = s_prefs.getString("name", DEFAULT_DEVICE_NAME);
        strncpy(m_config.deviceName, name.c_str(), sizeof(m_config.deviceName) - 1);
        m_config.deviceName[sizeof(m_config.deviceName) - 1] = '\0';

        m_config.wheelCircMm = s_prefs.getUShort("circ", DEFAULT_WHEEL_CIRC_MM);
        m_config.gearRatio = s_prefs.getFloat("ratio", DEFAULT_GEAR_RATIO);
        m_config.debounceMs = s_prefs.getUShort("deb", DEFAULT_DEBOUNCE_MS);
        m_config.inactivitySleepSec = s_prefs.getUShort("sleep", DEFAULT_INACTIVITY_SLEEP_SEC);
        m_config.adcMultiplier = s_prefs.getFloat("adc", DEFAULT_ADC_CAL_FACTOR);
        m_config.nvsVersion = storage::currentNvsVersion;
        sanitize();
    }
    s_prefs.end();
    if (needsSave) {
        saveConfig(m_config);
    }
#endif
    m_initialized = true;
    return true;
}

BikeConfig StorageManager::getConfig() const {
    return m_config;
}

void StorageManager::saveConfig(const BikeConfig& cfg) {
    m_config = cfg;
    m_config.nvsVersion = storage::currentNvsVersion;
    sanitize();

#ifndef NATIVE_TEST
    if (s_prefs.begin(PREFS_NAMESPACE, false)) {
        s_prefs.putString("name", m_config.deviceName);
        s_prefs.putUShort("circ", m_config.wheelCircMm);
        s_prefs.putFloat("ratio", m_config.gearRatio);
        s_prefs.putUShort("deb", m_config.debounceMs);
        s_prefs.putUShort("sleep", m_config.inactivitySleepSec);
        s_prefs.putFloat("adc", m_config.adcMultiplier);
        // Commit the schema marker last so an interrupted write is retried.
        s_prefs.putUShort("ver", m_config.nvsVersion);
        s_prefs.end();
    }
#endif
}

void StorageManager::resetDefaults() {
    loadDefaults();
#ifndef NATIVE_TEST
    if (s_prefs.begin(PREFS_NAMESPACE, false)) {
        s_prefs.clear();
        s_prefs.end();
    }
#endif
    saveConfig(m_config);
}
