# 07 – Gap-Analyse (Anforderung → Stand)

Legende: ✅ erfüllt · 🟡 teilweise · ❌ offen. „AP“ verweist auf `08_roadmap.md`.

## Organisatorisch

| Anforderung | Stand | Notiz | AP |
|---|---|---|---|
| Fork vorhanden | ✅ | `clHirschberg/MECH-M-3-IIoT` | – |
| `Robo-Chris` eingeladen | ❓ | im Repo nicht prüfbar → GitHub-Settings checken | AP8 |
| Abgabe-Link bis 27.11. 23:59 | ❌ | | AP8 |
| GitHub Pages (Quarto-Publish) läuft im Fork | ❓ | Workflow vorhanden; Pages/Actions im Fork aktiv? | AP8 |

## Termin 1 – MQTT-Firmware

| Anforderung | Stand | Notiz | AP |
|---|---|---|---|
| DHT11 auslesen, Fehler abfangen | ✅ | NaN → kein Publish | – |
| WLAN + MQTT zum Kurs-Broker | ✅ | | – |
| Topic laut Konvention (lowercase) | ❌ | `Kloepfer-Brose-Hirschberg-Kal` | AP1 |
| Payload `timestamp` ISO UTC | ❌ | `millis()`, kein NTP | AP1 |
| Payload `value` + `unit` | ❌ | `temperature`/`humidity` | AP1 |
| Status `ok` retained | 🟡 | retained, aber Metadaten statt Status | AP1 |
| Last Will `offline` | ❌ | | AP1 |
| Status `error` bei Sensorfehler | ❌ | | AP1 |
| Eindeutige Client-ID | ✅ | über Gruppenname | – |
| Struktur/Klassen/Konfiguration | 🟡 | Klassen ja; Implementierung in main.cpp; Altlast `config.h` | AP7 |

## Termin 2 – Netzwerk/REST

| Anforderung | Stand | Notiz | AP |
|---|---|---|---|
| Netzwerkkonfig per Datei, beim Start gelesen | 🟡 | `.env` zur **Compile-Zeit**, nicht zur Laufzeit; Pico-Vorlage nutzt `settings.toml` | AP1/F1 |
| `GET/POST /config` JSON | ✅ | Validierung + NVS-Persistenz (Plus: dauerhaft gespeichert) | – |
| `GET /status` | ✅ | vorläufiges Schema | AP2 |
| 400/404/405, Server läuft weiter | ✅ | | – |
| `/status` + Fehler in `openapi.yaml` | ❌ | nur `/config` dokumentiert | AP2 |
| Abstimmung Gesamtgruppe (Upstream-PR) | ❓ | Upstream prüfen | AP2 |
| Testfälle HTTP-API im Repo | ❌ | kein `tests/` | AP2 |
| Ausführung der Tests dokumentiert | ❌ | | AP2/AP6 |
| Absatz IT-Abteilung + Fernzugriff | ❌ | | AP6 |
| Webserver blockiert nicht | 🟡 | MQTT-Reconnect blockiert 5-s-weise | AP1 |

## Termin 3 – Cloud

| Anforderung | Stand | Notiz | AP |
|---|---|---|---|
| Start per `docker compose up` | ✅ | `docker compose up -d --build` | – |
| Doku: µC ins WLAN + Stack starten | 🟡 | Stack in README/infrastructure/README; µC-WLAN fehlt; `Dokumentation.qmd` leer | AP6 |
| genügend: lesen, speichern, visualisieren | ✅ | | – |
| befriedigend: echte DB | ✅ | SQLite (WAL) | – |
| gut: neue Sensoren automatisch | ✅ | `iiot/group/#`, Variable `group` | – |
| sehr gut: Sensorübersicht (Anzahl, letzte Verbindung, Status) | ❌ | ISSUES #1/#2 | AP3/AP4 |
| Bonus Mittelwert/Min/Max | ✅ | Grafana-Tabelle + Website-Kennzahlentabelle und Min/Max-Band | – |
| Robustheit fremder Formate | 🟡 | ISSUES #3/#4/#5 | AP3 |
| Ressourcenschonend, Laptop | ✅ | 4 schlanke Container | – |

## Querschnitt (Bewertungskriterien)

| Kriterium | Stand | Notiz |
|---|---|---|
| Doku Klarheit/Vollständigkeit | 🟡 | viel in READMEs, Abgabe-Doku leer, teils doppelt |
| Testfälle funktionieren | 🟡 | Cloud-Tests ja; HTTP-API-Tests fehlen |
| Codequalität | 🟡 | Cloud sehr sauber; `collector.py` monolithisch; Firmware Template-Reste |
| REST + OpenAPI | 🟡 | Implementierung gut, Spec unvollständig |
| Integration Firmware↔DB↔Analyse | 🟡 | funktioniert, aber nur dank Toleranz des Collectors gegenüber Nicht-Spec-Payload |

## Größte Hebel bis zur Abgabe (in Reihenfolge)

1. **Dokumentation.qmd** (Voraussetzung jeder Stufe) – AP6
2. **Sensorübersicht/Status** (*sehr gut* Cloud) – AP3 + AP4
3. **HTTP-API-Tests mit Schemathesis + OpenAPI `/status`** (*sehr gut* Termin 2) – AP2
4. **Firmware spec-konform** (Termin 1, Integration) – AP1
