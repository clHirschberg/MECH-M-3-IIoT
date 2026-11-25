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

    // Für GET /status: zuletzt erfolgreich gesendeter Wert und Zeitpunkt (millis)
    float lastValue = NAN;
    unsigned long lastPublishMs = 0;

    struct SensorMetadata {
        String type, unit, location, description;
        float min_value, max_value;
    } metadata;

    // Sendet einen Messwert auf iiot/group/<groupId>/sensor/<metric>/state.
    // NaN wird nicht gesendet. Rückgabe: true, wenn publiziert wurde.
    bool publishReading(const char* metric, float value, const char* unit);

    // Merkt sich den Wert für GET /status (last_value, last_publish_seconds_ago)
    void recordLastValue(float value) {
        lastValue = value;
        lastPublishMs = millis();
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

    void reconnectMqtt() {
        while (!mqttClient.connected()) {
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
                publishStatus("ok");
            } else {
                Serial.print("failed, rc=");
                Serial.print(mqttClient.state());
                Serial.println(" try again in 5 seconds");
                delay(5000);
            }
        }
    }

    void loop() {
        if (!mqttClient.connected()) {
            reconnectMqtt();
        }
        mqttClient.loop();

        if (millis() - lastSendTime > sendInterval) {
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
