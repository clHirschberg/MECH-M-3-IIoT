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

void Sensor::publishSensorData(float value) {
    JsonDocument doc;
    doc["sensorId"] = sensorId;
    doc["sensorName"] = sensorName;
    doc["timestamp"] = millis();
    buildSensorDataJson(doc, value); // Ruft die Methode der abgeleiteten Klasse auf
    String output;
    serializeJson(doc, output);
    if (mqttClient.publish(mqttTopicData.c_str(), output.c_str())) {
        lastValue = value;
        lastPublishMs = millis();
        Serial.println("Published data: " + output);
    } else {
        Serial.println("Failed to publish data.");
    }
}

void Sensor::publishMetadata() {
    JsonDocument doc;
    doc["sensorId"] = sensorId;
    doc["sensorName"] = sensorName;
    doc["type"] = metadata.type;
    doc["unit"] = metadata.unit;
    // ... (Rest der Metadaten)
    String output;
    serializeJson(doc, output);
    if (mqttClient.publish(mqttTopicMeta.c_str(), output.c_str(), true)) {
        Serial.println("Published metadata (retained): " + output);
    } else {
        Serial.println("Failed to publish metadata.");
    }
}

// ----- REST API (siehe docs/iot-specs/openapi.yaml) -----

static void sendJsonError(int code, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    String output;
    serializeJson(doc, output);
    server.send(code, "application/json", output);
}

void Sensor::setupRestApi() {
    // WebServer erwartet Callbacks ohne Objektbezug -> Lambdas, die 'this' einfangen.
    // Reihenfolge zählt: spezifische Methoden zuerst, HTTP_ANY fängt den Rest ab (405).
    server.on("/config", HTTP_GET, [this]() { handleGetSettings(); });
    server.on("/config", HTTP_POST, [this]() { handlePostSettings(); });
    server.on("/config", HTTP_ANY, []() { sendJsonError(405, "method not allowed"); });
    server.on("/status", HTTP_GET, [this]() { handleGetStatus(); });
    server.on("/status", HTTP_ANY, []() { sendJsonError(405, "method not allowed"); });
    server.onNotFound([]() { sendJsonError(404, "not found"); });

    server.begin();
    Serial.println("REST API listening on http://" + WiFi.localIP().toString() + "/" + "on port: 80");
}

void Sensor::handleRestApi() { server.handleClient(); }

void Sensor::handleGetSettings() {
    JsonDocument doc;
    doc["device_id"] = sensorName;
    doc["reading_interval_seconds"] = sendInterval / 1000;
    String output;
    serializeJson(doc, output);
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
    JsonVariant deviceId = doc["device_id"];
    if (interval.isNull() && deviceId.isNull()) {
        sendJsonError(400, "no known config field");
        return;
    }
    if (!interval.isNull() &&
        (!interval.is<long>() || interval.as<long>() < 1 || interval.as<long>() > 86400)) {
        sendJsonError(400, "reading_interval_seconds must be an integer between 1 and 86400");
        return;
    }
    if (!deviceId.isNull() && (!deviceId.is<const char*>() || strlen(deviceId.as<const char*>()) == 0)) {
        sendJsonError(400, "device_id must be a non-empty string");
        return;
    }

    preferences.begin("sensor_config", false);
    if (!interval.isNull()) {
        sendInterval = interval.as<unsigned long>() * 1000;
        preferences.putULong("sendInterval", sendInterval);
    }
    if (!deviceId.isNull()) {
        sensorName = deviceId.as<String>();
        preferences.putString("sensorName", sensorName);
    }
    preferences.end();

    handleGetSettings(); // 200 mit der jetzt gültigen Konfiguration
}

// Vorläufiges Schema - die Felder von /status werden noch in der Gesamtgruppe festgelegt
void Sensor::handleGetStatus() {
    JsonDocument doc;
    doc["device_id"] = sensorName;
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
    server.send(200, "application/json", output);
}
void Sensor::mqttCallback(char* topic, byte* payload, unsigned int length) { /* ... */ }