# 04 – Cloud-Stack (`compose.yaml`, `infrastructure/`)

Ausführliche Betriebsdoku: `infrastructure/README.md`. Offene Review-Befunde: `infrastructure/ISSUES.md` (#1–#9).

## compose.yaml (Projektname `iiot`)

| Dienst | Image/Build | Ports | Volumes | Besonderheiten |
|---|---|---|---|---|
| collector | `infrastructure/collector` (python:3.12.12-slim, UID 10001) | – | `measurements:/data`, `backups:/backups` | read_only, tmpfs /tmp, cap_drop ALL, Healthcheck über `/tmp/collector-health` (mtime < 30 s) |
| grafana | `grafana/grafana:13.3.0` | `127.0.0.1:${GRAFANA_PORT:-3000}` | `grafana`, `measurements:ro`, Provisioning ro | Plugin `frser-sqlite-datasource@4.0.6`, kein Sign-up/Anonym; `depends_on collector: service_healthy` |
| api | `infrastructure/api` (UID 10002) | `127.0.0.1:${API_PORT:-8000}` | `measurements:ro` | `GRAFANA_URL` Env; Healthcheck `/api/health` |
| website | `Website/` (node build → nginx-unprivileged 1.30.4) | `127.0.0.1:${WEBSITE_PORT:-4200}` → 8080 | – | `/api/` → `api:8000`, `/health` |

- `restart: on-failure:5` → kein Autostart nach Docker-/Rechner-Neustart (ISSUES #9).
- `.env` Pflicht: `MQTT_HOST`, `GRAFANA_ADMIN_PASSWORD` (`:?`-Syntax bricht sonst ab).

## Collector (`collector/collector.py`, ~210 Zeilen, eine Datei)

- paho-mqtt 2.1, `CallbackAPIVersion.VERSION2`, **MQTTv311**, `manual_ack=True` → QoS-1-ACK erst nach DB-Commit.
- Abo: `MQTT_TOPICS` (Default `iiot/group/#`, kommagetrennt), QoS 1. Zufällige Client-ID, falls leer.
- Reconnect-Schleife mit 5 s Pause; SIGTERM/SIGINT sauber.
- CLI: `--backup <neue Datei>` (SQLite-Backup-API + integrity_check), `--restore <datei>` (offline, prüft Schema `user_version=1`, löscht `-wal/-shm`).

### Schema (`PRAGMA user_version = 1`, WAL, `synchronous=FULL`)

```sql
messages(id PK, received_at REAL, topic TEXT, payload BLOB, qos INT, retained INT, parse_error TEXT)
readings(id PK, message_id → messages, time REAL, time_source TEXT ('device'|'received'),
         group_name TEXT, sensor_id TEXT, metric TEXT, value REAL, unit TEXT)
INDEX readings_time(time); INDEX readings_sensor_time(group_name, sensor_id, metric, time)
```

### `normalize(topic, payload, received)`

- Klartext `ok|error|offline` oder JSON-String davon → `[]` (nur Roharchiv).
- Sonst JSON-Objekt Pflicht, sonst `parse_error`.
- `group = topic.split('/')[2]` (**nicht** lowercase → ISSUES #3).
- `sensor = sensor_id | sensorId | ganzes Topic` (Fallback macht Legenden lang → ISSUES #9).
- `timestamp` String → ISO-Parse; ohne Zone → **ValueError, ganze Nachricht ohne readings** (ISSUES #4).
  Numerisch/fehlend → Empfangszeit, `time_source='received'`.
- `value` → metric = vorletztes Topic-Segment (`temperature`), unit = `unit` oder `''` (ISSUES #5).
- Zusätzlich Felder `temperature` (°C) und `humidity` (%).
- bool/NaN/inf/Nicht-Zahl → Fehler. Numerischer String `"21.5"` wird abgelehnt (ISSUES #5).
- **Status-Nachrichten werden nirgends ausgewertet** (ISSUES #1) → Stufe *sehr gut* fehlt.

## Lese-API (`api/app.py`, FastAPI)

| Endpoint | Zweck |
|---|---|
| `GET /api/health` | DB lesbar? (503 sonst) |
| `GET /api/config` | `{"grafana_url": …}` (validiert) |
| `GET /api/metadata` | DISTINCT `group_name, sensor_id, metric` (nur unterstützte Einheiten) |
| `GET /api/readings?from&to&groups[]&sensors[]&interval` | Bucket-Aggregation mean/min/max/count/received_count; K→°C; auto-Intervall (~500 Buckets); Limits: 64 Serien, 32 000 Punkte, Spanne/Intervall ≤ 2000; Zeit mit Zone Pflicht |

- Read-only (`mode=ro`, `query_only`), 10-s-Query-Timeout per Progress-Handler, parametrisiertes SQL.
- Swagger UI: `http://localhost:8000/docs`.
- Für *sehr gut* sinnvoll: neuer Endpoint `GET /api/devices` (aus Tabelle `devices`, siehe AP4).

## Grafana

- Datasource `iiot-sqlite` (uid), Pfad `/measurements/measurements.sqlite`, `mode=ro`.
- Dashboard `infrastructure/grafana/dashboards/overview.json`, uid `iiot-overview`, Titel „IIoT Übersicht“,
  Refresh 10 s, Zeitraum 1 h, Variable `group` (Multi, All) aus `readings`.
- Panels: Temperatur (timeseries, K→°C), Luftfeuchtigkeit (timeseries), Alle Messwerte im Zeitraum
  (table: count/min/max/avg → **Bonus-Kennzahlen**), Letzte empfangene Nachrichten (table, 50),
  Letzte Messwerte im gewählten Zeitraum (table).
- **Fehlt:** Anzahl Sensoren (stat), Sensorübersicht mit Status/letzter Verbindung (ISSUES #2).
- Provisioniert mit `allowUiUpdates: false` → Änderungen nur über die JSON-Datei.

## Tests

| Test | Befehl | Inhalt |
|---|---|---|
| Collector-Unit | `python3 -m unittest discover -s infrastructure/collector -v` | 5 Tests: Formate, Status/Metadaten, kaputte Payloads, paralleler Reader, Backup/Restore |
| API-Unit | `cd infrastructure/api && python3 -m unittest -v` (braucht fastapi + httpx) | Filter, Aggregation, K→°C, Injection, Validierung, Health/ro |
| Integration | `python3 infrastructure/tests/integration.py` (Docker) | Projekt `iiot-test`, eigener Mosquitto, Grafana :3300, Website :3301; 800 Nachrichten, Dashboard-Queries, Restart, Backup/Restore, Broker-Ausfall |

Hinweis: Die Integration setzt alle Panel-Queries ein (`${group:sqlstring}` → `'test-0'`). Neue Panels
müssen dort lauffähig sein (auch auf leerer/anderer Tabelle).

## Offene ISSUES (Kurzreferenz)

| # | Thema | Relevanz |
|---|---|---|
| 1 | Status aus `iiot/group/+/status` in Tabelle `devices` speichern (inkl. Migration v1→v2) | *sehr gut* |
| 2 | Dashboard: Anzahl Sensoren + Sensorübersicht (ohne Zeitfilter) | *sehr gut* |
| 3 | `group_name` lowercase, leere Gruppe verwerfen | Datenqualität |
| 4 | Zeitstempel ohne Zone = UTC; >300 s Abweichung → Empfangszeit | Datenqualität (andere Gruppen!) |
| 5 | `"21.5"` akzeptieren, fehlende Unit bei temperature = °C, K beim Parsen umrechnen, ROUND | Datenqualität |
| 6 | Healthcheck nur bei MQTT-Verbindung; Grafana `service_started` | Betrieb |
| 7 | WAL + Read-only-Mount nach Stop/Restore verifizieren, sonst `journal_mode=DELETE` | Robustheit |
| 8 | `docs/Dokumentation.qmd` befüllen | **Voraussetzung jeder Stufe** |
| 9 | Aufräumen (README, MQTT-3.1.1-Begründung, restart-Policy, sensor_id-Fallback, collector.py aufteilen) | Codequalität |
