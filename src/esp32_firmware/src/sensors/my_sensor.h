#ifndef MY_SENSOR_H
#define MY_SENSOR_H

#include "Sensor.h"
#include <DHT.h>

// Konfiguration für den konkreten Sensor
#define DHT_SENSOR_PIN 4
#define DHT_SENSOR_TYPE DHT11

class MyTempSensor : public Sensor {
private:
    DHT dht; // Das sensorspezifische Objekt

public:
    // Der Konstruktor ruft den Basis-Konstruktor auf und leitet alle Infos weiter
    MyTempSensor(String location, String description, bool useSecureMqtt)
        : Sensor("temperature", "°C", -10.0, 50.0, location, description, useSecureMqtt),
          dht(DHT_SENSOR_PIN, DHT_SENSOR_TYPE)
    {
        Serial.println("Initializing DHT11 sensor...");
        dht.begin();
    }

    // Implementierung der reinen virtuellen Funktion der Basisklasse:
    // Jede Messgröße geht als eigene Nachricht auf ihr eigenes Topic.
    void readAndPublish() override {
        // Die DHT-Library liest den Sensor max. alle 2 s, der zweite Aufruf nutzt denselben Messzyklus
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();

        // GET /status meldet die zuletzt gesendete Temperatur als last_value
        if (publishReading("temperature", temperature, "°C")) {
            recordLastValue(temperature);
        }
        if (publishReading("humidity", humidity, "%")) {
            recordLastHumidity(humidity);
        }

        // Ungültige Messung (NaN, z.B. Sensor abgesteckt) -> Status "error",
        // nach Erholung wieder "ok". Gesendet wird nur bei einem Wechsel.
        setStatus(isnan(temperature) || isnan(humidity) ? "error" : "ok");
    }
};

#endif // MY_SENSOR_H