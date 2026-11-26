# 03 – ESP32-Firmware (`src/esp32_firmware/`)

## Aufbau

| Datei | Rolle |
|---|---|
| `platformio.ini` | `env:esp32dev`, Arduino, libs: PubSubClient ^2.8, ArduinoJson ^7, DHT sensor library ^1.4, Adafruit Unified Sensor; `extra_scripts = pre:load_env.py` |
| `load_env.py` | liest `.env` → Makros `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_BROKER_HOST`, `MQTT_USER`, `MQTT_PASSWORD` (Pflicht, sonst Build-Abbruch) |
| `.env.example` | Vorlage dieser 5 Werte |
| `include/config_secure.h` | Ports 1883/8883, `MQTT_CLIENT_ID_PREFIX = "ESP32_Sensor_"`, Platzhalter-CA |
| `config.h` (Root) | **Altlast**: doppelte Definitionen (WLAN-Platzhalter, Prefix, CA); liegt nicht im Include-Pfad → ungenutzt, verwirrend → löschen |
| `src/Sensor.h` | Basisklasse `Sensor`: WLAN, MQTT, Preferences, Topics, Loop |
| `src/sensors/my_sensor.h` | `MyTempSensor : Sensor` mit DHT11 an **GPIO 4** |
| `src/main.cpp` | globale Objekte, `setup()/loop()`, Implementierung publish/REST |

## Ablauf

1. `setup()`: Serial 115200 → `setupWifiAndMqtt()` (blockierend bis WLAN da) → `setupRestApi()`.
2. `loop()`: `mySensor.loop()` (MQTT reconnect **blockierend** mit `delay(5000)`, `mqttClient.loop()`,
   alle `sendInterval` ms messen und publishen) → `handleRestApi()` (`server.handleClient()`).
3. Bei Connect: `publishMetadata()` → retained auf Status-Topic.

## Konfiguration

- Compile-Time (`.env`): WLAN, Broker-Host, MQTT-User/Passwort.
- Runtime/NVS (`Preferences`, Namespace `sensor_config`): `sensorName` (Default `"DefaultSensor"`),
  `sendInterval` in ms (Default **5000** = 5 s; Spec-Beispiel 60 s).
- Hartkodiert in `Sensor.h`: `groupId = "Kloepfer-Brose-Hirschberg-Kal"`; `sensorId = "ESP32_Sensor_" + groupId`
  (= MQTT-Client-ID). In `main.cpp`: Ort `"Wohnzimmer"`, Beschreibung, `useSecureMqtt=false`.
- WLAN-Wechsel vor Ort erfordert **neu flashen** → widerspricht der Storyline „vor Ort einfach konfigurierbar“
  (siehe 09, Entscheidung F1).

## MQTT – Ist-Zustand

Messwert-Topic: `iiot/group/Kloepfer-Brose-Hirschberg-Kal/sensor/temperature/state`, QoS 0, nicht retained:

```json
{"sensorId":"ESP32_Sensor_Kloepfer-Brose-Hirschberg-Kal","sensorName":"DefaultSensor",
 "timestamp":123456,"temperature":22.5,"humidity":48}
```

Status-Topic `iiot/group/Kloepfer-Brose-Hirschberg-Kal/status`, retained, nur beim Connect:

```json
{"sensorId":"…","sensorName":"DefaultSensor","type":"temperature","unit":"°C"}
```

## MQTT – Abweichungen von `asyncapi.yaml` / `conventions.md`

| # | Soll | Ist | Fix |
|---|---|---|---|
| M1 | Topic nur Kleinbuchstaben | `Kloepfer-Brose-Hirschberg-Kal` | `groupId` lowercase; besser konfigurierbar machen |
| M2 | `value` (number) + `unit` (`°C`) Pflicht | `temperature`, `humidity`, kein `unit` | `doc["value"]=t; doc["unit"]="°C";` (humidity als Zusatzfeld behalten – Collector versteht beides) |
| M3 | `timestamp` UTC ISO 8601 | `millis()` | NTP: `configTime(0,0,"pool.ntp.org")`, `getLocalTime()`, `strftime("%Y-%m-%dT%H:%M:%SZ")`; ohne gültige Zeit Feld weglassen oder nicht senden |
| M4 | `sensor_id` (snake_case) optional | `sensorId`, `sensorName` | `sensor_id` = `device_id` aus Config |
| M5 | Status `{"status":"ok"}` (DeviceStatus) retained, QoS 1 | Metadaten-JSON ohne `status` | Payload `{"status":"ok","timestamp":…,"sensor_id":…}` |
| M6 | Last Will `offline` retained | fehlt | `mqttClient.connect(id, user, pw, statusTopic, 1, true, "{\"status\":\"offline\"}")` (oder Klartext `offline` – im Team festlegen) |
| M7 | `error` bei ungültigen Sensordaten | fehlt | bei NaN Status `error` publishen, nach Erholung wieder `ok` (nur bei Zustandswechsel) |
| M8 | MQTT 5.0 laut conventions | PubSubClient = 3.1.1 | Hinnehmen und in Doku begründen (Broker akzeptiert 3.1.1) |
| M9 | Client-ID eindeutig | `ESP32_Sensor_<gruppe>` – eindeutig, solange nur 1 Gerät/Gruppe | ok; optional MAC-Suffix (`WiFi.macAddress()`) |

Hinweis: PubSubClient `publish()` ist immer QoS 0; Will kann QoS 1 haben. Retained bei `publish(topic, payload, true)`.

## REST-API – Ist-Zustand (gut umgesetzt)

| Route | Verhalten |
|---|---|
| `GET /config` | `{"device_id": sensorName, "reading_interval_seconds": sendInterval/1000}` |
| `POST /config` | JSON-Body, validiert alles vor Übernahme: `reading_interval_seconds` int 1…86400, `device_id` nicht-leerer String; mind. ein bekanntes Feld; speichert in NVS; Antwort 200 mit neuer Config |
| `GET /status` | **vorläufig**: `device_id, uptime_seconds, wifi_connected, wifi_rssi, ip, mqtt_connected, last_value, last_publish_seconds_ago` |
| `OPTIONS /config`, `/status` | CORS-Preflight 204 |
| andere Methode | 405 `{"error":"method not allowed"}` |
| unbekannter Pfad | 404 `{"error":"not found"}` |
| Fehler | 400 `{"error": "..."}` (fehlender Body, invalid JSON, Validierung) |

Offene Punkte REST:
- `device_id` ändert nur `sensorName` – Topic/Client-ID bleiben gleich (ok, aber dokumentieren).
- `/status` liefert kein Feld `status` (`ok|error|offline`) und keine Luftfeuchte/letzten Zeitstempel → mit OpenAPI-Schema abstimmen.
- CORS-Whitelist hartkodiert (Port 4200) – bei anderem `WEBSITE_PORT` Firmware ändern.
- Während MQTT-Reconnect (`while … delay(5000)`) ist der Webserver blockiert → nicht-blockierend umbauen
  (Reconnect-Versuch höchstens alle 5 s per `millis()`), damit `/status` auch ohne Broker antwortet.
- `setupWifiAndMqtt()` blockiert endlos ohne WLAN – akzeptabel, aber Doku-Hinweis (Serial-Log).

## Kleinere Code-Qualitätspunkte

- Kommentar in `Sensor.h` „Die restlichen Funktionen … aus Platzgründen nicht aufgeführt“ ist Template-Rest → entfernen.
- Implementierungen in `main.cpp` statt `Sensor.cpp` (Template sagt „idealerweise Sensor.cpp“) → für Modularität verschieben.
- `mqttCallback` leer, `espClientSecure`/CA ungenutzt (Kurs-Broker ohne TLS) – ok, aber kommentieren.
- `metadata.min_value/max_value/location/description` werden nicht genutzt.
- `buildSensorDataJson` liest Luftfeuchte separat (zweiter DHT-Read) – DHT-Lib cached 2 s, ok.
- Remote-Branch `origin/esp32_code` enthält eine ältere Variante von `Sensor.h` → prüfen/verwerfen.

## Build/Flash

```bash
cd src/esp32_firmware
cp .env.example .env    # Werte eintragen
pio run -t upload
pio device monitor      # 115200 Baud
```
