# 08 – Roadmap / Arbeitspakete

Priorisiert für die Abgabe am **27.11.2026, 23:59**. Jedes AP ist in sich abgeschlossen und kann in
einer frischen Sitzung bearbeitet werden. Nach Abschluss: Checkboxen hier abhaken, Abweichungen unter
„Notizen“ eintragen, ggf. `infrastructure/ISSUES.md` abhaken.

Empfohlene Reihenfolge: **AP1 → AP2 → AP3 → AP4 → AP6 → AP5 → AP7 → AP8**
(AP1 zuerst, weil Firmware-Format AP2-Tests, AP3-Parser und die Doku beeinflusst. Wenn die Hardware
gerade nicht verfügbar ist: AP3/AP4 vorziehen.)

Parallelisierbar im Team: AP1 (Firmware, braucht ESP32) ‖ AP3+AP4 (Cloud) ‖ AP6 (Doku-Gerüst).

---

## AP1 – Firmware spezifikationskonform (Termin 1) · Prio hoch · Hardware nötig

Dateien: `src/esp32_firmware/src/{Sensor.h,main.cpp,sensors/my_sensor.h}`, ggf. `.env.example`, `load_env.py`.

- [x] Gruppenname lowercase (`kloepfer-brose-hirschberg-kal`), idealerweise aus `.env` (`GROUP_ID`) statt hartkodiert
- [x] NTP (`configTime(0, 0, "pool.ntp.org", "time.google.com")`), `timestamp` als `YYYY-MM-DDTHH:MM:SSZ`
- [x] Payload: `{"timestamp","value","unit":"°C","sensor_id", "humidity"}` (humidity als Zusatz behalten)
- [x] Status-Payload `{"status":"ok","timestamp","sensor_id"}` retained nach Connect
- [x] Last Will `{"status":"offline"}` retained, QoS 1 im `connect()`
- [x] `error` bei Sensor-NaN, zurück auf `ok` bei Erholung (nur bei Zustandswechsel publishen)
- [x] MQTT-Reconnect nicht-blockierend (Versuch alle 5 s via `millis()`), Webserver bleibt bedienbar
- [x] `/status` um `status` (ok/error/offline-Logik lokal) ergänzen, passend zu AP2
- [ ] Optional: Default-Intervall 60 s statt 5 s (Spec-Beispiel) – im Team entscheiden (F4)
- [ ] Optional: WLAN zur Laufzeit konfigurierbar (F1)

Akzeptanz: `mosquitto_sub -t 'iiot/group/kloepfer-brose-hirschberg-kal/#' -v` zeigt spec-konforme
Payloads; nach Abstecken erscheint `offline`; Collector speichert `time_source='device'`.
Achtung: Alte Mixed-Case-Gruppe bleibt in der DB → AP3 (lowercase) löst das.

## AP2 – OpenAPI vervollständigen + HTTP-API-Tests (Termin 2) · Prio hoch

Dateien: `docs/iot-specs/openapi.yaml`, neu `tests/` (Root), ggf. `pyproject.toml` (pdm dev-group).

- [x] Upstream prüfen: gibt es einen gemergten `/status`-PR? Falls ja, Felder übernehmen und Firmware angleichen
- [x] `openapi.yaml`: `/status`, Schemas `Status`, `Error`, Grenzen, 400-Responses, POST-200 mit `Config` (siehe 06)
- [x] Spec validieren (z.B. `npx @redocly/cli lint` oder Swagger Editor)
- [x] `tests/test_device_api.py` (pytest): GET/POST config, Roundtrip, 400 (kaputtes JSON, falscher Typ, Grenzen), 404, 405, `/status`-Felder; Basis-URL per Env `DEVICE_URL`
- [x] Schemathesis: `schemathesis run docs/iot-specs/openapi.yaml --url $DEVICE_URL` (+ Checks `all`); als pytest-Integration oder Skript `tests/run_schemathesis.sh`
- [x] Tests ohne Gerät überspringen (`pytest.skip` wenn `DEVICE_URL` fehlt) – damit CI/Kollegen nicht rot sehen
- [x] Optional: REST-Client-Datei `tests/device.http` zusätzlich (schnelle manuelle Tests)
- [x] Ausführung in README/Doku beschreiben

Akzeptanz: `DEVICE_URL=http://<esp-ip> pdm run pytest tests -v` grün gegen echtes Gerät; Schemathesis ohne Fehler.
Hinweis: POST mit Fuzzing ändert die Gerätekonfiguration → danach Intervall zurücksetzen (Fixture).

## AP3 – Collector: Status, Datenqualität (ISSUES #1, #3, #4, #5) · Prio hoch

Dateien: `infrastructure/collector/collector.py`, `test_collector.py`.

- [x] Tabelle `devices` + Migration `user_version` 1 → 2 (auch `restore()` muss v2 akzeptieren)
- [x] Status-Parsing: Klartext / JSON-String / JSON-Objekt mit `status`, `strip().lower()`, sonst `unknown`; leerer Payload ignorieren
- [x] Upsert `last_seen/last_value/unit` bei Messwerten; Status und Messwert überschreiben sich nicht
- [x] `group_name.lower()`, leere Gruppe verwerfen
- [x] Zeitstempel ohne Zone = UTC; Abweichung > 300 s → Empfangszeit
- [x] `"21.5"` akzeptieren, fehlende Unit bei temperature = °C, K → °C beim Parsen
- [x] Bestehende Daten: Migration lowercased `group_name` in `readings` (UPDATE) – im Team entscheiden (F5)
- [x] Unit-Tests laut Akzeptanzkriterien in ISSUES #1/#3/#4/#5
- [ ] Optional (ISSUES #9): Aufteilung in Module `parsing.py`, `storage.py`, `mqtt_client.py`, `cli.py`

Akzeptanz: `python3 -m unittest discover -s infrastructure/collector -v` grün; Integrationstest grün.

## AP4 – Sensorübersicht in Grafana (+ optional Website) (ISSUES #2) · Prio hoch · setzt AP3 voraus

Dateien: `infrastructure/grafana/dashboards/overview.json`, `infrastructure/tests/integration.py`,
optional `infrastructure/api/app.py` + `Website/src/app/…`.

- [x] Stat-Panel „Anzahl Sensoren“: `known_sensors`, `active_sensors` (Messwert < 300 s), leere DB → 0
- [x] Table-Panel „Sensorübersicht“ ohne Zeitfilter: group, status, status_updated_at, last_seen, last_value, unit, active; Value-Mappings (ok grün, error/offline rot, unknown orange)
- [x] Integrationstest: Status-Nachrichten publishen, Panel-Queries prüfen (auch auf leerer DB)
- [x] Optional: `GET /api/devices` + Tab „Geräte“ in der Website (mobil tauglich)
- [x] Optional: Gruppenvariable aus `devices` statt `readings`

Akzeptanz: Dashboard zeigt alle Gruppen inkl. inaktiver; `python3 infrastructure/tests/integration.py` grün.

## AP5 – Betrieb/Robustheit (ISSUES #6, #7, restart) · Prio mittel

- [ ] Heartbeat nur bei bestehender MQTT-Verbindung; Grafana/API `depends_on: service_started`
- [ ] Integrationstest: falsches Passwort → Collector unhealthy, Grafana erreichbar
- [ ] WAL nach Collector-Stop und nach `--restore` mit Grafana ro testen; sonst `journal_mode=DELETE`
- [ ] Restart-Policy entscheiden (`unless-stopped` vs. dokumentiertes `on-failure:5`) (F6)

## AP6 – Abgabe-Dokumentation `docs/Dokumentation.qmd` (ISSUES #8) · Prio sehr hoch

- [x] Zusammenfassung + Architekturdiagramm (Mermaid in Quarto: ESP32 → Broker → Collector → SQLite → Grafana/API → Website)
- [x] Anleitung User: Hardware (Verkabelung GPIO 4), `.env` der Firmware, Flashen (`pio run -t upload`), Serial-Monitor, IP ablesen, Konfiguration über Website/curl
- [x] Anleitung User: Stack starten (`.env`, `docker compose up -d --build`, URLs, Login), Backup, Fehlerbehebung (inkl. `grafana cli admin reset-admin-password`)
- [x] Empfehlungen IT-Abteilung: Whitelist ausgehend (1883/TCP zu 158.180.44.197, 123/UDP NTP, 53 DNS, DHCP), eingehend 80/TCP nur LAN, 2,4-GHz-WLAN, keine Client-Isolation; Fernzugriff (VPN/WireGuard, Reverse-Tunnel, Port-Forwarding-Risiken, keine Auth/TLS, MQTT-basierte Konfiguration als Alternative)
- [x] Anleitung Developer: Struktur (`infrastructure/` statt `src/cloud`), Tests (Unit, Integration, HTTP-API/Schemathesis, vitest), pdm/Quarto, Spec-Abweichungen (MQTT 3.1.1) begründet
- [x] `infrastructure/README.md` verlinken statt duplizieren
- [x] `pdm run quarto render` läuft durch

## AP7 – Aufräumen (ISSUES #9 + Firmware) · Prio niedrig

- [ ] `src/esp32_firmware/config.h` löschen (ungenutzt), Template-Kommentare entfernen
- [ ] Optional: `Sensor.cpp` statt Implementierung in `main.cpp`
- [ ] Remote-Branch `origin/esp32_code` prüfen und löschen/mergen (F7)
- [x] `infrastructure/README.md`: MQTT-3.1.1-Begründung korrigieren
- [x] `sensor_id`-Fallback statt ganzem Topic `'-'`
- [x] Root-README: Abschnitt „Projektüberblick unserer Gruppe“ oben, Links auf Doku
- [x] Ungenutzte Pico-Vorlage: in Doku als „nicht verwendet“ kennzeichnen (nicht löschen, gehört zur Aufgabenstellung)

## AP8 – Finale Prüfung & Abgabe · Prio sehr hoch · am 27.11.

- [ ] Frischer Clone → README-Anleitung Schritt für Schritt durchspielen
- [ ] Alle Tests grün (Collector, API, Integration, Website `npm test`, HTTP-API gegen Gerät)
- [ ] `.env`-Dateien nicht im Git (`git ls-files | grep .env` → nur `.env.example`)
- [ ] GitHub Pages des Forks erreichbar, Doku aktuell
- [ ] `Robo-Chris` hat Zugriff
- [ ] Abgabe-Link einreichen

---

## Notizen / Abweichungen

(hier nach jedem AP ergänzen)

### Abgleich mit Kurs-Website (26.11.2026)

- Firmware (AP1) weitgehend erledigt in `origin/main` c441325 (Carl) – lokal noch nicht gepullt. Offen: `error`-Status bei NaN, nicht-blockierender Reconnect prüfen, `/status` noch „vorläufiges Schema“.
- Repo ist öffentlich, `Robo-Chris` ist Collaborator. GitHub Pages ist nicht aktiviert (laut Aufgabe nicht Pflicht).
- Pflicht laut Abgabeseite: Root-README mit festen Abschnitten (Gruppe, Video, Erreichter Stand, Überblick, Mikrocontroller einrichten, Container starten, Dashboard nutzen, HTTP-API, Hinweise IT, Sicherheit) → fehlt komplett (README ist noch Kurs-README + Angular-Anleitung).
- Pflicht: `tests/` mit HTTP-API-Tests (AP2) → fehlt. `openapi.yaml` ohne `/status`.
- Stufe „sehr gut“ (Geräteübersicht aus `iiot/group/+/status`) fehlt: Collector verwirft Status-Nachrichten (AP3/AP4).
- `docker-compose.yml` wird verlangt, wir haben `compose.yaml` (funktioniert mit `docker compose up`; Pfad im README nennen oder umbenennen).
- `src/raspi_firmware/settings.toml` enthält nur Platzhalter (ok).

### Umsetzung 26.11.2026 (Branch `oskl/feat/abgabe`, noch nicht committet)

- Upstream `jhumci/main` (a24ca25: Abgabe-Anforderungen, openapi 1.2.0 verbindlich, Status überall JSON) per `git merge --no-commit` übernommen; Remote `upstream` angelegt. README-Konflikt aufgelöst (README wird zum Schluss neu geschrieben).
- Firmware (kompiliert mit PlatformIO, auf Hardware **nicht getestet**): POST /config nur noch `reading_interval_seconds` (2–86400), `device_id` per POST → 400 (Spec 1.2.0); /status nach Schema `DeviceStatus` (`last_telemetry`, `last_humidity_percent`) + Zusatzfelder `status`, `wifi_rssi`, `ip`; `error`/`ok` bei Zustandswechsel; MQTT-Reconnect nicht blockierend (alle 5 s), Messung nur bei MQTT-Verbindung; 405 mit `Allow`-Header (Schemathesis ST204).
- `openapi.yaml`: 1.2.0 + Gruppen-Ergänzungen (maximum 86400, Error-Schema, 405, Zusatzfelder), redocly-valide (nur Stilwarnungen wegen fehlender security/operationId).
- `tests/`: pytest (`test_device_api.py`, 35 Fälle inkl. „neues Intervall wird genutzt“), Schemathesis (`test_openapi_schemathesis.py`), `device.http` (REST Client). pdm-Gruppe `test` in `pyproject.toml`, `pdm.lock` neu gelockt. Gegen einen Python-Mock grün, **gegen echtes Gerät noch offen**: `DEVICE_URL=http://<ip> pdm run pytest -v`.
- Collector: Schema v2 mit `devices`, Migration (lowercase + Rückfüllen aus Roharchiv), Status-Parsing, Zeitstempel-/Unit-Toleranz, `sensor_id`-Fallback `-`. 17 Unit-Tests grün; Migration an Kopie der echten DB getestet.
- Grafana: Panels „Anzahl Sensoren“ (stat) und „Geräteübersicht“ (table) oben, Kennzahlen gerundet, Gruppenvariable aus `devices`. API `GET /api/devices` + Website-Komponente „Sensorübersicht“ im Messdaten-Tab. Integrationstest (Docker) grün, Website-Tests (vitest) grün.
- Nicht umgesetzt: AP5 (Healthcheck/Restart-Policy, Risiko beim Start), AP7-Rest (config.h, Modulaufteilung), AP6/README (bewusst zum Schluss).
- Laufender Stack muss neu gebaut werden: `docker compose up -d --build` (Collector migriert die DB beim Start).

### Dokumentation 26.11.2026 (AP6 + README, noch nicht committet)

- Root-`README.md` komplett neu nach Pflichtabschnitten der Abgabeseite (Gruppe, Video, Stand, Überblick mit Mermaid, Mikrocontroller, Container, Dashboard, HTTP-API, IT, Sicherheit). Compose-Pfad `compose.yaml` genannt (nicht umbenannt).
- `docs/Dokumentation.qmd` gefüllt (Zusammenfassung, Architektur/Topics, User-Anleitung inkl. Fehlerbehebung, IT-Whitelist + Fernzugriffsdiskussion, Developer: Struktur, Firmware erweitern, Datenmodell, Tests, Abweichungen MQTT 3.1.1/.env statt settings.toml, KI-Einsatz). `quarto render` (ganze Site) ohne Warnungen.
- `infrastructure/README.md`: veraltete Aussagen („Firmware unverändert“, Status-Metadaten, MQTT-3.1.1-Begründung) korrigiert, `devices` ergänzt.
- **Offen (vom Team auszufüllen):** im README Vornamen Brose und vollständiger Name „Kal…“ (`<!-- TODO -->`), Video-Link (`TODO Link einfügen`). Beanspruchte Stufen (T2 sehr gut, T3 sehr gut + Bonus) nach Gerätetest bestätigen; README-Beispielausgaben (Serial, `device_id` `iot-gruppe-5`) beim Test gegenprüfen.
- Danach AP8 (frischer Clone, Tests gegen Gerät, Commit/PR nach `main`, Abgabe).

### Hardware-Test 26.11.2026 abends (ganze Strecke)

- Firmware geflasht (`upload_speed = 115200` in `platformio.ini` ergänzt, Default brach mit „Failed to start stub flasher“ ab). Serial: WLAN, IP 192.168.68.62, NTP, MQTT ok.
- Broker: Payloads spec-konform, `POST /config` 10 s sichtbar im Takt, Reset gehalten → `{"status":"offline"}` nach ~22 s retained, danach wieder `ok`.
- `DEVICE_URL=http://192.168.68.62 pytest`: 35/35 grün. Schemathesis-Fälle `QUERY` und `multipart/form-data` ohne Boundary scheitern an Arduino-`WebServer` (Verbindung ohne Antwort) → im pytest als Robustheitsprüfung behandelt (`tests/test_openapi_schemathesis.py`), in README/Doku als Grenze dokumentiert; CLI meldet sie weiterhin.
- Stack mit `docker compose up -d --build` neu gebaut (Backup vorher: `/backups/pre-v2-2026-11-26.sqlite` im Volume). DB jetzt `user_version` 2, `devices` gefüllt (10 Gruppen), keine Mixed-Case-Gruppen, `time_source='device'`, `/api/devices` ok, Grafana-Dashboard mit neuen Panels.
- Namen im README eingetragen (Oskar Klöpfer, Carl Hirschberg, Can Kal, Daniel Brose). Offen: Video-Link, Commit/PR nach `main`, AP8.
- Testumgebung lokal: `pdm` nicht installiert; Tests liefen mit venv (Python 3.10) und `env -u PYTHONPATH` (ROS-humble-PYTHONPATH bricht pytest-Plugins).

### Website: Doku-Tab + Darkmode (26.11.2026 spät, noch nicht committet)

- Neuer Tab **Dokumentation** (`/doku`, `Website/src/app/docs/documentation.ts`): rendert `docs/Dokumentation.qmd` mit `marked`, Inhaltsverzeichnis (ab lg), Mermaid lazy geladen (Diagramm auf der Website hochkant TD statt LR), externe Links in neuem Tab.
- `Website/scripts/sync-docs.mjs` kopiert die qmd vor `npm start/build/test` nach `Website/public/docs/dokumentation.md` (Quarto-```{mermaid} → ```mermaid, relative Links → GitHub). Kopie ist eingecheckt, weil der Docker-Build nur `Website/` sieht → nach Doku-Änderungen `npm run sync-docs` und committen.
- Darkmode: `ThemeService` (Hell/Dunkel/System, localStorage `iiot-theme`, Klasse `.dark` auf `<html>`), Startskript in `index.html` gegen Aufblitzen, `dark:`-Klassen in allen Templates, Chart.js-Achsen/Raster und Serienfarben theme-abhängig, `@tailwindcss/typography` für die Doku. Quarto-Seite: `theme: light cosmo / dark darkly`.
- Neue Deps: `marked`, `mermaid`, `@tailwindcss/typography`; `allowedCommonJsDependencies` in `angular.json` für Mermaid.
- Website-Tests 14/14 grün, `npm run build` ok, Container neu gebaut und hell/dunkel/mobil per Headless-Chromium geprüft; `quarto render` ok.
