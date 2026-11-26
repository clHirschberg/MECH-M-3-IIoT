# 10 – Befehle (Build, Start, Test, Doku)

Alle Befehle aus dem Repo-Root, sofern nicht anders angegeben.

## Docker-Stack

```bash
cp .env.example .env                 # MQTT_PASSWORD + GRAFANA_ADMIN_PASSWORD eintragen
docker compose up -d --build         # alles bauen und starten
docker compose ps
docker compose logs --tail=100 collector
docker compose stop | start
docker compose down                  # Container weg, Volumes bleiben
# NICHT: docker compose down -v  (löscht Messdaten!)
```

URLs: Website http://localhost:4200 · Grafana http://localhost:3000 (Dashboards → IIoT → IIoT Übersicht)
· API-Doku http://localhost:8000/docs

DB-Größe / ad-hoc SQL:

```bash
docker compose exec collector python -c "import os; print(os.path.getsize('/data/measurements.sqlite'))"
docker compose exec collector python -c "import sqlite3; d=sqlite3.connect('/data/measurements.sqlite'); print(d.execute('SELECT group_name, COUNT(*) FROM readings GROUP BY 1').fetchall())"
```

Backup/Restore: siehe `infrastructure/README.md` (Abschnitt „Backup und Wiederherstellung“).

## Tests

```bash
python3 -m unittest discover -s infrastructure/collector -v
(cd infrastructure/api && pip install -r requirements.txt httpx && python3 -m unittest -v)
python3 infrastructure/tests/integration.py           # Docker, Projekt iiot-test, Ports 3300/3301
(cd Website && npm test)                              # vitest
# nach AP2:
DEVICE_URL=http://<esp-ip> pdm run pytest tests -v
schemathesis run docs/iot-specs/openapi.yaml --url http://<esp-ip>
```

## Broker beobachten

```bash
mosquitto_sub -h 158.180.44.197 -p 1883 -u bobm -P '<passwort>' -t 'iiot/group/+/#' -v
```

## ESP32

```bash
cd src/esp32_firmware
cp .env.example .env
pio run                  # bauen
pio run -t upload        # flashen
pio device monitor       # Serial 115200
curl -v http://<esp-ip>/config
curl -v -X POST http://<esp-ip>/config -H 'Content-Type: application/json' -d '{"reading_interval_seconds":10}'
curl -v http://<esp-ip>/status
```

## Website lokal (Hot Reload)

```bash
docker compose stop website
docker compose up -d collector api grafana
cd Website && npm install && ng serve -o
```

## Quarto-Doku

```bash
pdm install
pdm run quarto preview
pdm run quarto render        # Abnahmekriterium AP6
```

## Spezifikationen prüfen

```bash
npx @asyncapi/cli validate docs/iot-specs/asyncapi.yaml
npx @redocly/cli lint docs/iot-specs/openapi.yaml
```
