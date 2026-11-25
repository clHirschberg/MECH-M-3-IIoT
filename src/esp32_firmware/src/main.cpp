#include <Arduino.h>
#include "sensors/my_sensor.h" // Unser konkreter Sensor
#include "Sensor.h"           // Die Sensor-Basisklasse

// ----- Globale Definitionen -----
// Diese Objekte werden von der Sensor-Klasse als 'extern' erwartet.
WiFiClient espClient;              // Unverschlüsselt (Port 1883)
WiFiClientSecure espClientSecure;  // TLS (Port 8883)
PubSubClient mqttClient(espClient);
WebServer server(80);
Preferences preferences;

// ----- Sensor-Instanz erstellen -----
// Hier wird der Sensor konfiguriert.
// Parameter: Ort, Beschreibung, true/false für sichere/unsichere MQTT-Verbindung
// Der Kurs-Broker läuft unverschlüsselt auf Port 1883 -> false
MyTempSensor mySensor("Wohnzimmer", "Temperatur und Luftfeuchte", false);


void setup() {
    Serial.begin(115200);
    delay(1000); // Gibt der seriellen Konsole Zeit, sich zu verbinden

    // Das WIFI_SSID, WIFI_PASSWORD und MQTT_BROKER_HOST Makro wird
    // zur Kompilierzeit von platformio.ini hier eingesetzt.
    mySensor.setupWifiAndMqtt();
    mySensor.setupRestApi();

    Serial.println("Setup complete. Starting main loop...");
}

void loop() {
    mySensor.loop();
    mySensor.handleRestApi();
}


// ----- Implementierung der restlichen Sensor-Methoden -----
// Da sie im Header nur deklariert wurden, müssen sie hier implementiert werden.
// Alternativ könnte man eine Sensor.cpp-Datei erstellen.

// ----- Zeit (NTP, UTC) -----

void Sensor::syncTime() {
    // Offset 0 / keine Sommerzeit: Die Systemzeit läuft in UTC
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Waiting for NTP time sync");
    // Vor dem Sync steht die Uhr auf 1970 -> blockieren, bis eine plausible Zeit da ist
    while (time(nullptr) < 1700000000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nTime synced: " + isoTimestampUtc());
}

String Sensor::isoTimestampUtc() {
    time_t now = time(nullptr);
    struct tm utc;
    gmtime_r(&now, &utc);
    char buf[sizeof "2026-11-25T18:30:00Z"];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &utc);
    return String(buf);
}

// ----- MQTT (siehe docs/iot-specs/conventions.md) -----

bool Sensor::publishReading(const char* metric, float value, const char* unit) {
    if (isnan(value)) {
        Serial.println(String("Invalid ") + metric + " reading, not published.");
        return false;
    }
    JsonDocument doc;
    doc["timestamp"] = isoTimestampUtc();
    doc["sensor_id"] = deviceId;
    doc["value"] = value;
    doc["unit"] = unit;
    String output;
    serializeJson(doc, output);
    String topic = mqttTopicBase + "/sensor/" + metric + "/state";
    if (mqttClient.publish(topic.c_str(), output.c_str())) {
        Serial.println("Published " + topic + ": " + output);
        return true;
    }
    Serial.println("Failed to publish " + topic);
    return false;
}

void Sensor::publishStatus(const char* status) {
    JsonDocument doc;
    doc["status"] = status;
    doc["timestamp"] = isoTimestampUtc();
    doc["sensor_id"] = deviceId;
    String output;
    serializeJson(doc, output);
    if (mqttClient.publish(mqttTopicStatus.c_str(), output.c_str(), true)) {
        Serial.println("Published status (retained): " + output);
    } else {
        Serial.println("Failed to publish status.");
    }
}

// ----- REST API (siehe docs/iot-specs/openapi.yaml) -----

static void sendCorsHeaders() {
    // Hier weitere Frontend-Adressen ergänzen, falls das Dashboard anders gehostet wird.
    const String origin = server.header("Origin");
    server.sendHeader("Vary", "Origin");
    if (origin == "http://localhost:4200" || origin == "http://127.0.0.1:4200") {
        server.sendHeader("Access-Control-Allow-Origin", origin);
        server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    }
}

static void handleCorsPreflight() {
    sendCorsHeaders();
    server.send(204, "text/plain", "");
}

static void sendJsonError(int code, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    String output;
    serializeJson(doc, output);
    sendCorsHeaders();
    server.send(code, "application/json", output);
}

void Sensor::setupRestApi() {
    const char* headers[] = {"Origin"};
    server.collectHeaders(headers, 1);
    // WebServer erwartet Callbacks ohne Objektbezug -> Lambdas, die 'this' einfangen.
    // Reihenfolge zählt: spezifische Methoden zuerst, HTTP_ANY fängt den Rest ab (405).
    server.on("/config", HTTP_OPTIONS, handleCorsPreflight);
    server.on("/config", HTTP_GET, [this]() { handleGetSettings(); });
    server.on("/config", HTTP_POST, [this]() { handlePostSettings(); });
    server.on("/config", HTTP_ANY, []() { sendJsonError(405, "method not allowed"); });
    server.on("/status", HTTP_OPTIONS, handleCorsPreflight);
    server.on("/status", HTTP_GET, [this]() { handleGetStatus(); });
    server.on("/status", HTTP_ANY, []() { sendJsonError(405, "method not allowed"); });
    server.onNotFound([]() { sendJsonError(404, "not found"); });

    server.begin();
    Serial.println("REST API listening on http://" + WiFi.localIP().toString() + "/" + "on port: 80");
}

void Sensor::handleRestApi() { server.handleClient(); }

void Sensor::handleGetSettings() {
    JsonDocument doc;
    doc["device_id"] = deviceId;
    doc["reading_interval_seconds"] = sendInterval / 1000;
    String output;
    serializeJson(doc, output);
    sendCorsHeaders();
    server.send(200, "application/json", output);
}

void Sensor::handlePostSettings() {
    // WebServer legt den Request-Body unter dem Argument "plain" ab
    if (!server.hasArg("plain")) {
        sendJsonError(400, "missing JSON body");
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) {
        sendJsonError(400, "invalid JSON");
        return;
    }

    // Erst alles validieren, dann übernehmen -> keine halb angewendete Konfiguration
    JsonVariant interval = doc["reading_interval_seconds"];
    JsonVariant newDeviceId = doc["device_id"];
    if (interval.isNull() && newDeviceId.isNull()) {
        sendJsonError(400, "no known config field");
        return;
    }
    if (!interval.isNull() &&
        (!interval.is<long>() || interval.as<long>() < 1 || interval.as<long>() > 86400)) {
        sendJsonError(400, "reading_interval_seconds must be an integer between 1 and 86400");
        return;
    }
    if (!newDeviceId.isNull() && (!newDeviceId.is<const char*>() || strlen(newDeviceId.as<const char*>()) == 0)) {
        sendJsonError(400, "device_id must be a non-empty string");
        return;
    }

    preferences.begin("sensor_config", false);
    if (!interval.isNull()) {
        sendInterval = interval.as<unsigned long>() * 1000;
        preferences.putULong("sendInterval", sendInterval);
    }
    if (!newDeviceId.isNull()) {
        deviceId = newDeviceId.as<String>();
        preferences.putString("sensorName", deviceId);
    }
    preferences.end();

    // Neue sensor_id auch im Last Will: sauber trennen, loop() verbindet neu
    // und sendet "ok" mit der neuen ID.
    if (!newDeviceId.isNull()) {
        mqttClient.disconnect();
    }

    handleGetSettings(); // 200 mit der jetzt gültigen Konfiguration
}

// Vorläufiges Schema - die Felder von /status werden noch in der Gesamtgruppe festgelegt
void Sensor::handleGetStatus() {
    JsonDocument doc;
    doc["device_id"] = deviceId;
    doc["uptime_seconds"] = millis() / 1000;
    doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["mqtt_connected"] = mqttClient.connected();
    if (isnan(lastValue)) {
        doc["last_value"] = nullptr;
        doc["last_publish_seconds_ago"] = nullptr;
    } else {
        doc["last_value"] = lastValue;
        doc["last_publish_seconds_ago"] = (millis() - lastPublishMs) / 1000;
    }
    String output;
    serializeJson(doc, output);
    sendCorsHeaders();
    server.send(200, "application/json", output);
}
void Sensor::mqttCallback(char* topic, byte* payload, unsigned int length) { /* ... */ }
