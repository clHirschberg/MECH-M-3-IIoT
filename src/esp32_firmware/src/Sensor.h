#ifndef SENSOR_H
#define SENSOR_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include "config_secure.h" // Inkludiert Zertifikat und Port-Definitionen

// Globale Objekte, da sie von der Basisklasse genutzt werden
extern WiFiClient espClient;              // Unverschlüsselt (Port 1883)
extern WiFiClientSecure espClientSecure;  // TLS (Port 8883)
extern PubSubClient mqttClient;
extern WebServer server;
extern Preferences preferences;

class Sensor {
protected:
    String mqttClientId;  // Client-ID am Broker (muss dort eindeutig sein)
    String groupId;       // Topic-Teil iiot/group/<groupId>/... (Kleinbuchstaben, conventions.md)
    String deviceId;      // Wird als "sensor_id" gesendet, per REST /config als "device_id" änderbar
    unsigned long sendInterval;
    unsigned long lastSendTime = 0;
    String mqttTopicBase;   // iiot/group/<groupId>
    String mqttTopicStatus; // iiot/group/<groupId>/status
    bool useSecureConnection; // Das neue Flag für MQTTS

    // Für GET /status (openapi.yaml 1.2.0): letzte gesendete Temperatur und Feuchte
    float lastValue = NAN;
    String lastTimestamp;
    float lastHumidity = NAN;

    // Aktueller Gerätestatus ("ok" oder "error"), wird nur bei Änderung neu gesendet
    String currentStatus = "ok";
    unsigned long lastMqttAttemptMs = 0;
    static const unsigned long MQTT_RETRY_MS = 5000;

    struct SensorMetadata {
        String type, unit, location, description;
        float min_value, max_value;
    } metadata;

    // Sendet einen Messwert auf iiot/group/<groupId>/sensor/<metric>/state.
    // NaN wird nicht gesendet. Rückgabe: true, wenn publiziert wurde.
    bool publishReading(const char* metric, float value, const char* unit);

    // Merkt sich die zuletzt gesendete Temperatur für GET /status (last_telemetry)
    void recordLastValue(float value) {
        lastValue = value;
        lastTimestamp = isoTimestampUtc();
    }

    // Merkt sich die zuletzt gemessene Feuchte für GET /status (last_humidity_percent)
    void recordLastHumidity(float value) { lastHumidity = value; }

    // Setzt den Status ("ok"/"error") und sendet ihn nur bei einem Wechsel (retained)
    void setStatus(const char* status) {
        if (currentStatus == status) return;
        currentStatus = status;
        if (mqttClient.connected()) publishStatus(status);
    }

public:
    // Der Konstruktor nimmt nun das Flag für die Verbindungsart entgegen
    Sensor(String type, String unit, float minVal, float maxVal, String loc, String desc, bool isSecure) {
        useSecureConnection = isSecure;

        groupId = "kloepfer-brose-hirschberg-kal";
        mqttClientId = String(MQTT_CLIENT_ID_PREFIX) + groupId;

        preferences.begin("sensor_config", false);
        // Key bleibt "sensorName", damit bereits gespeicherte Werte erhalten bleiben
        deviceId = preferences.getString("sensorName", "iot-gruppe-5");
        sendInterval = preferences.getULong("sendInterval", 5000);
        preferences.end();

        metadata.type = type;
        metadata.unit = unit;
        metadata.min_value = minVal;
        metadata.max_value = maxVal;
        metadata.location = loc;
        metadata.description = desc;

        mqttTopicBase = "iiot/group/" + groupId;
        mqttTopicStatus = mqttTopicBase + "/status";
    }

    virtual ~Sensor() {}

    // Muss von der abgeleiteten Klasse implementiert werden:
    // Sensor auslesen und jeden Messwert per publishReading() senden.
    virtual void readAndPublish() = 0;

    void setupWifiAndMqtt() {
        Serial.print("Connecting to WiFi: ");
        Serial.println(WIFI_SSID); // Kommt aus platformio.ini
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Kommt aus platformio.ini
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

        syncTime();

        // Logik für sichere vs. unsichere Verbindung
        if (useSecureConnection) {
            Serial.println("Setting up secure MQTTS connection.");
            espClientSecure.setCACert(MQTT_BROKER_CA_CERT); // Zertifikat aus config_secure.h
            mqttClient.setClient(espClientSecure);
            mqttClient.setServer(MQTT_BROKER_HOST, MQTT_SECURE_PORT); // Sicherer Port
        } else {
            Serial.println("Setting up insecure MQTT connection. NO CERTIFICATE WILL BE USED.");
            mqttClient.setClient(espClient);
            mqttClient.setServer(MQTT_BROKER_HOST, MQTT_INSECURE_PORT); // Unsicherer Port
        }

        mqttClient.setCallback(mqttCallback);
    }

    // Nicht blockierend: höchstens alle MQTT_RETRY_MS ein Verbindungsversuch,
    // damit die REST-API auch ohne Broker erreichbar bleibt.
    void reconnectMqtt() {
        if (lastMqttAttemptMs != 0 && millis() - lastMqttAttemptMs < MQTT_RETRY_MS) return;
        lastMqttAttemptMs = millis();
        {
            Serial.print("Attempting MQTT connection...");
            // Last Will: Der Broker sendet "offline" (retained), wenn die Verbindung abreißt.
            // Ohne timestamp, da der Zeitpunkt des Abbruchs beim Verbinden unbekannt ist.
            JsonDocument will;
            will["status"] = "offline";
            will["sensor_id"] = deviceId;
            String willPayload;
            serializeJson(will, willPayload);
            // MQTT_USER und MQTT_PASSWORD kommen aus platformio.ini (.env)
            if (mqttClient.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                                   mqttTopicStatus.c_str(), 1, true, willPayload.c_str())) {
                Serial.println("connected!");
                // Überschreibt den retained Last Will mit dem aktuellen Status
                publishStatus(currentStatus.c_str());
            } else {
                Serial.print("failed, rc=");
                Serial.print(mqttClient.state());
                Serial.println(" try again in 5 seconds");
            }
        }
    }

    void loop() {
        if (!mqttClient.connected()) {
            reconnectMqtt();
        }
        mqttClient.loop();

        if (mqttClient.connected() && millis() - lastSendTime >= sendInterval) {
            readAndPublish();
            lastSendTime = millis();
        }
    }

    void publishStatus(const char* status);
    void setupRestApi();
    void handleRestApi();

private:
    void syncTime();
    static String isoTimestampUtc();
    void handleGetSettings();
    void handlePostSettings();
    void handleGetStatus();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
};

// Implementierungen der verbleibenden Funktionen müssen in einer .cpp-Datei sein,
// idealerweise in Sensor.cpp, oder zur Vereinfachung direkt in main.cpp.

#endif // SENSOR_H
