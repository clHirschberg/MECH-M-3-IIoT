# _agents – Projektkontext für die weitere Umsetzung

Dieser Ordner enthält die Analyse des Repositorys (Stand **25.11.2026**, Commit `b544f08` auf `main`).
Er ist als Einstiegspunkt für neue Arbeitssitzungen (Mensch oder KI-Agent) gedacht: Zuerst diese
Datei lesen, dann die für das jeweilige Arbeitspaket relevanten Dateien.

> **Abgabe: Freitag, 27.11.2026, 23:59 Uhr** (Link zum Fork `clHirschberg/MECH-M-3-IIoT`).
> Ab Analysedatum bleiben ca. 2,5 Tage. Die Roadmap ist danach priorisiert.

## Dateien

| Datei | Inhalt |
|---|---|
| [01_aufgabenstellung.md](01_aufgabenstellung.md) | Storyline, Anforderungen Termin 1–3, Bewertungskriterien und -stufen, Abgabe |
| [02_architektur.md](02_architektur.md) | Gesamtsystem, Datenfluss, Komponenten, Ports, Repo-Struktur |
| [03_firmware_esp32.md](03_firmware_esp32.md) | ESP32-Firmware im Detail, Abweichungen von der Spezifikation |
| [04_cloud_stack.md](04_cloud_stack.md) | Docker-Stack: Collector, SQLite-Schema, Lese-API, Grafana, Tests |
| [05_website_angular.md](05_website_angular.md) | Angular-Website (Geräte-API-Tab, Messdaten-Tab) |
| [06_specs_konventionen.md](06_specs_konventionen.md) | MQTT-/REST-Konventionen, AsyncAPI, OpenAPI, Ist vs. Soll |
| [07_gap_analyse.md](07_gap_analyse.md) | Anforderung für Anforderung: erfüllt / teilweise / offen |
| [08_roadmap.md](08_roadmap.md) | Priorisierte Arbeitspakete (AP) mit Checkboxen und Akzeptanzkriterien |
| [09_offene_fragen_risiken.md](09_offene_fragen_risiken.md) | Entscheidungen, die das Team treffen muss, und Risiken |
| [10_entwicklung_befehle.md](10_entwicklung_befehle.md) | Befehle für Build, Start, Tests, Doku |

## Kurzfazit

- **Termin 3 (Cloud)** ist am weitesten: Collector → SQLite → Grafana + eigene Lese-API + Angular-Website,
  per `docker compose up -d --build` startbar, mit Tests. Stufen *genügend* bis *gut* sind erfüllt,
  für *sehr gut* fehlt die **Geräte-/Statusübersicht** (siehe `infrastructure/ISSUES.md` #1/#2).
- **Termin 2 (REST)** ist auf dem ESP32 funktional (`/config` GET/POST, `/status` GET, 400/404/405, CORS,
  Persistenz in NVS). Es fehlen **`/status` in `openapi.yaml`** und **Testfälle für die HTTP-API** (`tests/`).
- **Termin 1 (MQTT)**: Die Firmware sendet zwar, aber **nicht spezifikationskonform**
  (Payload ohne `value`/`unit`, `timestamp` = `millis()`, Status-Topic enthält Metadaten statt `ok`,
  kein Last Will, Gruppenname mit Großbuchstaben).
- **Dokumentation**: `docs/Dokumentation.qmd` ist noch die leere Vorlage – Voraussetzung für *jede* Bewertungsstufe.

## Arbeitsweise (aus der globalen CLAUDE.md)

- Nach jedem abgeschlossenen Arbeitspaket: Status in `08_roadmap.md` abhaken, Abweichungen/offene
  Punkte notieren, Memory aktualisieren, dann anhalten.
- Bezüge zu `infrastructure/ISSUES.md` in Commits referenzieren, z.B. `fix(cloud): … (ISSUES #3)`.
- Geheimnisse (`.env`, `src/esp32_firmware/.env`) niemals committen oder in diese Dateien schreiben.
