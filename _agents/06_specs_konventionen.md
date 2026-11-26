# 06 – Spezifikationen und Konventionen (`docs/iot-specs/`)

## conventions.md (verbindlich)

- Topic Messwerte: `iiot/group/{nachname1-nachname2-…}/sensor/temperature/state`, nur Kleinbuchstaben.
- Topic Status: `iiot/group/{…}/status`, **retained**, Last Will = `offline`.
- Payload JSON, Zeitstempel **UTC ISO 8601** (`2025-08-28T10:00:00Z`), Einheit als String (`"°C"`),
  Statuswerte `ok | error | offline`.
- Broker `158.180.44.197:1883`, User `bobm`, Passwort aus der Vorlesung (nur in `.env`!), kein TLS, MQTT 5.0.
- REST: HTTP, Port 80, `http://<deviceIp>/`; `/config` GET/POST; Feldnamen = Schlüssel der `settings.toml`
  (`device_id`, `reading_interval_seconds`). Weitere Felder + `/status` per Gesamtgruppen-PR.

## asyncapi.yaml (2.6.0, v1.1.0)

`TemperatureReading` – required `timestamp` (date-time), `value` (number), `unit` (`°C`|`K`);
optional `sensor_id`, `status`.

`DeviceStatus` – required `status`; optional `timestamp`, `sensor_id`. Binding QoS 1, retain true.

Unklar in der Spec: Status als Klartext (`ok`) oder JSON (`{"status":"ok"}`)? Schema sagt Objekt,
Doku-Text/Hinweise sagen „`ok` auf dem Status-Topic“. Der Collector muss **beides** verstehen (ISSUES #1);
die Firmware sollte das Schema (JSON-Objekt) senden.

## openapi.yaml (3.0.3, v1.1.0) – Ist

Nur `/config` GET (200, Schema `Config{device_id, reading_interval_seconds}`) und POST (Body `Config`, 200 ohne Schema).

## openapi.yaml – Soll (Vorschlag, passend zur Firmware)

Vor Änderung prüfen, ob im Upstream (`jhumci/MECH-M-3-IIoT`) ein gemergter Gruppen-PR für `/status`
existiert – dann **dessen Felder** übernehmen und Firmware angleichen (Pflicht laut Termin 2).

```yaml
paths:
  /config:
    get:  200 → Config
    post: requestBody Config (minProperties: 1, additionalProperties: false)
          200 → Config (gültige neue Konfiguration)
          400 → Error
          405 → Error (auf Pfadebene schwer in OAS 3.0 abbildbar; in description erwähnen)
  /status:
    get:  200 → Status
components:
  schemas:
    Config:
      device_id: string, minLength 1
      reading_interval_seconds: integer, minimum 1, maximum 86400
    Status:
      required: [device_id, uptime_seconds, wifi_connected, mqtt_connected]
      device_id: string
      status: enum [ok, error, offline]        # neu, analog MQTT-Status
      uptime_seconds: integer ≥ 0
      wifi_connected: boolean
      wifi_rssi: integer (dBm)
      ip: string (ipv4)
      mqtt_connected: boolean
      last_value: number, nullable
      last_publish_seconds_ago: integer, nullable
    Error:
      required: [error]
      error: string
```

Damit Schemathesis sinnvoll prüft: Grenzen (`minimum/maximum`), `nullable`, `additionalProperties`,
Fehler-Responses und `servers`-URL per CLI überschreibbar (`--url http://<ip>`).

## Konsistenz Ist ↔ Spec (Übersicht)

| Thema | Spec | Firmware | Collector | Website |
|---|---|---|---|---|
| Messwert-Felder | value/unit/timestamp | temperature/humidity/millis | versteht beide | – |
| Status-Payload | `{"status":…}` | Metadaten-JSON | ignoriert | – |
| Gruppenname | lowercase | Mixed Case | übernimmt roh | zeigt roh |
| `/status` | nicht spezifiziert | 8 Felder | – | untypisiert |
| MQTT-Version | 5.0 | 3.1.1 | 3.1.1 | – |
