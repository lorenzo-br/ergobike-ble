#include "web_portal.h"
#include "web_assets.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <ArduinoJson.h>

static AsyncWebServer s_server(80);
static DNSServer s_dnsServer;
#define DNS_PORT 53
#endif

WebPortal::WebPortal()
    : m_storage(nullptr)
    , m_sensor(nullptr)
    , m_battery(nullptr)
    , m_running(false)
{
}

void WebPortal::setupRoutes() {
#ifndef NATIVE_TEST
    // 1. Captive Portal / Root HTML
    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });

    // Captive portal fallback redirects
    s_server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/");
    });
    s_server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/");
    });
    s_server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/");
    });

    // 2. Status API (Live JSON)
    s_server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        StaticJsonDocument<256> doc;
        doc["cadence"] = m_sensor ? m_sensor->getCadenceRpm() : 0.0f;
        doc["speed"] = m_sensor ? m_sensor->getSpeedKmh() : 0.0f;
        doc["pulses"] = m_sensor ? m_sensor->getTotalPulses() : 0;
        doc["batteryPct"] = m_battery ? m_battery->getPercentage() : 100;
        doc["batteryV"] = m_battery ? m_battery->getVoltage() : 4.2f;
        doc["calibrating"] = m_sensor ? m_sensor->isCalibrating() : false;
        doc["calibPulses"] = m_sensor ? m_sensor->getCalibrationPulseCount() : 0;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // 3. Current Config API
    s_server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!m_storage) {
            request->send(500, "text/plain", "Storage not ready");
            return;
        }
        BikeConfig cfg = m_storage->getConfig();
        StaticJsonDocument<256> doc;
        doc["name"] = cfg.deviceName;
        doc["ratio"] = cfg.gearRatio;
        doc["circ"] = cfg.wheelCircMm;
        doc["debounce"] = cfg.debounceMs;
        doc["sleep"] = cfg.inactivitySleepSec;
        doc["adc"] = cfg.adcMultiplier;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // 4. Save Config Endpoint (JSON Body)
    AsyncCallbackJsonWebHandler* saveHandler = new AsyncCallbackJsonWebHandler("/api/save", [this](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!m_storage || !json.is<JsonObject>()) {
            request->send(400, "application/json", "{\"status\":\"error\"}");
            return;
        }
        JsonObject obj = json.as<JsonObject>();
        BikeConfig cfg = m_storage->getConfig();

        if (obj.containsKey("name")) {
            strncpy(cfg.deviceName, obj["name"].as<const char*>(), sizeof(cfg.deviceName) - 1);
        }
        if (obj.containsKey("ratio")) {
            cfg.gearRatio = obj["ratio"].as<float>();
        }
        if (obj.containsKey("circ")) {
            cfg.wheelCircMm = obj["circ"].as<uint16_t>();
        }
        if (obj.containsKey("debounce")) {
            cfg.debounceMs = obj["debounce"].as<uint16_t>();
        }
        if (obj.containsKey("sleep")) {
            cfg.inactivitySleepSec = obj["sleep"].as<uint16_t>();
        }
        if (obj.containsKey("adc")) {
            cfg.adcMultiplier = obj["adc"].as<float>();
        }

        m_storage->saveConfig(cfg);
        if (m_sensor) m_sensor->setConfig(cfg);
        if (m_battery) m_battery->setConfig(cfg);

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    s_server.addHandler(saveHandler);

    // 5. Calibration Start
    s_server.on("/api/calibrate/start", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (m_sensor) {
            m_sensor->startCalibration();
        }
        request->send(200, "application/json", "{\"status\":\"calibrating\"}");
    });

    // 6. Calibration Finish
    s_server.on("/api/calibrate/finish", HTTP_POST, [this](AsyncWebServerRequest* request) {
        uint16_t turns = 10;
        if (request->hasParam("turns")) {
            turns = request->getParam("turns")->value().toInt();
            if (turns == 0) turns = 10;
        }
        uint32_t pulses = m_sensor ? m_sensor->getCalibrationPulseCount() : 0;
        float newRatio = m_sensor ? m_sensor->finishCalibration(turns) : DEFAULT_GEAR_RATIO;

        if (m_storage) {
            BikeConfig cfg = m_storage->getConfig();
            cfg.gearRatio = newRatio;
            m_storage->saveConfig(cfg);
        }

        StaticJsonDocument<128> doc;
        doc["status"] = "ok";
        doc["ratio"] = newRatio;
        doc["pulses"] = pulses;
        doc["turns"] = turns;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // 7. Factory Reset
    s_server.on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (m_storage) {
            m_storage->resetDefaults();
            BikeConfig cfg = m_storage->getConfig();
            if (m_sensor) m_sensor->setConfig(cfg);
            if (m_battery) m_battery->setConfig(cfg);
        }
        request->send(200, "application/json", "{\"status\":\"reset\"}");
    });

    // 8. Reboot Device
    s_server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "Rebooting...");
        delay(500);
        ESP.restart();
    });

    // 9. OTA Firmware Update Handler
    s_server.on("/update", HTTP_POST, [](AsyncWebServerRequest* request) {
        bool updateFailed = Update.hasError();
        AsyncWebServerResponse* response = request->beginResponse(
            updateFailed ? 500 : 200, 
            "text/plain", 
            updateFailed ? "FAIL" : "OK"
        );
        response->addHeader("Connection", "close");
        request->send(response);
        if (!updateFailed) {
            delay(500);
            ESP.restart();
        }
    }, [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
        if (!index) {
            Serial.printf("[OTA] Update Start: %s\n", filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        }
        if (!Update.hasError()) {
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
        }
        if (final) {
            if (Update.end(true)) {
                Serial.printf("[OTA] Update Success: %u bytes\n", index + len);
            } else {
                Update.printError(Serial);
            }
        }
    });

    // Catch all 404
    s_server.onNotFound([](AsyncWebServerRequest* request) {
        request->redirect("/");
    });
#endif
}

void WebPortal::begin(StorageManager* storage, SensorReader* sensor, BatteryMonitor* battery) {
    m_storage = storage;
    m_sensor = sensor;
    m_battery = battery;

#ifndef NATIVE_TEST
    WiFi.mode(WIFI_AP);
    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP(AP_SSID, AP_PASS);

    Serial.println("[WIFI] Access Point started: " AP_SSID);
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.softAPIP());

    s_dnsServer.start(DNS_PORT, "*", local_ip);
    setupRoutes();
    s_server.begin();
    Serial.println("[HTTP] Web Portal Server running on port 80.");
#endif

    m_running = true;
}

void WebPortal::update() {
#ifndef NATIVE_TEST
    if (m_running) {
        s_dnsServer.processNextRequest();
    }
#endif
}

void WebPortal::stop() {
#ifndef NATIVE_TEST
    s_server.end();
    s_dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
#endif
    m_running = false;
}

bool WebPortal::isRunning() const {
    return m_running;
}
