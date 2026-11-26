# 02 – Architektur und Repo-Struktur

## Datenfluss

```
 DHT11 ──GPIO4── ESP32 (PlatformIO/Arduino)
                   │  MQTT 3.1.1 (PubSubClient), TCP 1883, unverschlüsselt
                   │  iiot/group/<gruppe>/sensor/temperature/state   (Messwerte)
                   │  iiot/group/<gruppe>/status                      (retained)
                   ▼
           Kurs-Broker 158.180.44.197:1883 (User bobm)
                   │  Abo iiot/group/# (QoS 1), alle Gruppen
                   ▼
  ┌──────────────── docker compose (Projekt "iiot") ─────────────────┐
  │ collector (Python, paho-mqtt) ──schreibt──► Volume measurements   │
  │                                             /data/measurements.sqlite
  │ grafana 13.3.0 + frser-sqlite ──liest ro──►      (WAL)            │
  │ api (FastAPI)                 ──liest ro──►                       │
  │ website (nginx + Angular) ──/api/──► api:8000                     │
  └───────────────────────────────────────────────────────────────────┘
        127.0.0.1:4200 Website · 127.0.0.1:3000 Grafana · 127.0.0.1:8000 API (/docs)

 Browser (Tab "Geräte-API") ──HTTP direkt, CORS──► ESP32 :80  /config, /status
```

Wichtig: Die Website spricht den ESP32 **direkt aus dem Browser** an (gleiches LAN nötig). Der ESP32
erlaubt CORS nur für `http://localhost:4200` und `http://127.0.0.1:4200`.

## Komponenten

| Komponente | Pfad | Technik | Zustand |
|---|---|---|---|
| ESP32-Firmware | `src/esp32_firmware/` | PlatformIO, Arduino, PubSubClient, ArduinoJson 7, DHT, WebServer, Preferences | funktional, nicht spec-konform (→ 03) |
| Pico-W-Vorlage | `src/raspi_firmware/` | CircuitPython | **unverändert Vorlage**, wird nicht genutzt |
| Collector | `infrastructure/collector/` | Python 3.12, paho-mqtt 2.1.0, sqlite3 | produktiv, Tests vorhanden (→ 04) |
| Lese-API | `infrastructure/api/` | FastAPI 0.162, uvicorn | produktiv, Tests vorhanden |
| Grafana | `infrastructure/grafana/` | Grafana 13.3.0, Plugin `frser-sqlite-datasource@4.0.6`, provisioniert | Dashboard `iiot-overview` |
| Website | `Website/` | Angular 21 (standalone, Signals), Tailwind 4, Chart.js 4.6.1, vitest; nginx-unprivileged im Container | 2 Tabs (→ 05) |
| Specs | `docs/iot-specs/` | AsyncAPI 2.6.0, OpenAPI 3.0.3, conventions.md | Upstream-Stand; `/status` fehlt |
| Kurs-Doku | `index.qmd`, `docs/*.qmd`, `_quarto.yml` | Quarto, via pdm | Vorlage; `Dokumentation.qmd` leer |
| CI | `.github/workflows/` | Quarto-Publish auf gh-pages, AsyncAPI-Validierung | aus Upstream |

## Repo-Struktur (relevant)

```
.
├── README.md                     # Start mit Docker, Angular lokal, Quarto
├── compose.yaml                  # 4 Dienste: collector, grafana, api, website
├── .env.example / .env           # MQTT-, Grafana-, Port-Konfiguration (.env gitignored)
├── pyproject.toml / pdm.lock     # nur quarto-cli
├── _quarto.yml, index.qmd        # Kurs-Website
├── docs/
│   ├── Termin_1..3*.qmd, Materialien.qmd
│   ├── Dokumentation.qmd         # UNSERE Abgabe-Doku (leer!)
│   └── iot-specs/{conventions.md, asyncapi.yaml, openapi.yaml}
├── src/
│   ├── esp32_firmware/           # unsere Firmware
│   │   ├── platformio.ini, load_env.py, .env.example (.env gitignored)
│   │   ├── config.h              # Altlast, nicht im Include-Pfad, ungenutzt
│   │   ├── include/config_secure.h
│   │   └── src/{main.cpp, Sensor.h, sensors/my_sensor.h}
│   └── raspi_firmware/           # Vorlage Pico (ungenutzt)
├── infrastructure/
│   ├── README.md                 # ausführliche Betriebsdoku des Stacks
│   ├── ISSUES.md                 # Review-Befunde #1–#9 (offen)
│   ├── collector/{collector.py, test_collector.py, Dockerfile}
│   ├── api/{app.py, test_api.py, requirements.txt, Dockerfile}
│   ├── grafana/{provisioning/…, dashboards/overview.json}
│   └── tests/{integration.py, compose.test.yaml, mosquitto.conf}
└── Website/                      # Angular-App
```

Es gibt **kein** `tests/` im Root (HTTP-API-Tests aus Termin 2 fehlen) und **kein** `src/cloud`
(die Vorlage schlägt es vor; wir nutzen `infrastructure/` – in der Doku begründen).

## Ports / Netzwerk

| Richtung | Ziel | Port/Proto | Wer |
|---|---|---|---|
| ausgehend | 158.180.44.197 | 1883/TCP (MQTT) | ESP32, Collector |
| ausgehend | DNS des Netzes | 53/UDP | ESP32 (nur falls Hostnamen, z.B. NTP-Pool) |
| ausgehend | NTP-Server (geplant) | 123/UDP | ESP32 (nach AP1) |
| eingehend (LAN) | ESP32 | 80/TCP (HTTP) | Laptop/Website |
| lokal | 127.0.0.1 | 4200 / 3000 / 8000 | Website / Grafana / API |

## Git

- Branch `main` (Default). Remote-Branch `origin/esp32_code` mit 1 Commit („Add: Wifi and Mqqt config“,
  ändert `Sensor.h` + löscht `.env.example`) – **nicht gemergt**, vermutlich veraltet; vor Abgabe klären/löschen.
- Commit-Konvention: Conventional Commits (`feat:`, `fix:`, `docs(cloud):` …).
- Beitragende: Oskar (Website, Cloud-Stack), Carl L. Hirschberg (Firmware, Review).
