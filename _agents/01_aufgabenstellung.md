# 01 – Aufgabenstellung und Bewertung

Quellen: `index.qmd`, `docs/Termin_1_Grundkonzepte_IoT.qmd`, `docs/Termin_2_Netzwerke.qmd`,
`docs/Termin_3_Cloud.qmd`, `docs/Materialien.qmd`, `docs/Dokumentation.qmd`.

## Rahmen

- Lehrveranstaltung: Industrial Internet of Things (MECH-M-DUAL-3-IOT), MCI, WS 2026.
- Teil 1 (Termine 1–3): Christoph Egger (GitHub: `Robo-Chris`) – wird separat bewertet.
- Gruppenarbeit; unsere Gruppe: Kloepfer, Brose, Hirschberg, Kal… (4 Personen, siehe Topic-Name).
- Fork: `https://github.com/clHirschberg/MECH-M-3-IIoT` (Upstream `jhumci/MECH-M-3-IIoT`).
- **Abgabe:** Link zum Fork, spätestens **Fr, 27.11.2026, 23:59**. `Robo-Chris` muss eingeladen sein.
- Bestehen: 60 % insgesamt (gemittelt mit Teil 2 nach SWS).
- Generative KI ausdrücklich erlaubt (reflektierter Einsatz).

## Storyline (Kurz)

yourmuesli.at hat Qualitätsprobleme durch Temperatur/Luftfeuchte. QS-Leiter will bis 27.11. die
Temperaturwerte **aller Standorte weltweit** auf einem Dashboard (iPhone) sehen. Lösung: günstige
Mikrocontroller (Pico W / ESP32 + DHT11), zentral in Innsbruck entwickelt, verschickt, vor Ort von
**technisch wenig versierten Nutzer:innen** angeschlossen/konfiguriert. Fernüberwachung des
Sensorzustands und Änderung von Einstellungen soll möglich sein.

## Termin 1 – Grundkonzepte (MQTT-Firmware)

Ziel: Firmware sendet DHT11-Messwerte an den Kurs-Broker **gemäß `conventions.md` und `asyncapi.yaml`**.

- Fork + `Robo-Chris` einladen.
- Sensor verkabeln (Pico: GP22; ESP32-Vorlage: GPIO 4).
- Meilensteine: LED → Sensor lesen → WLAN → MQTT-Nachricht → **Spezifikation erfüllen** → Refactoring.
- Meilenstein 5 konkret: Payload JSON mit `timestamp` (UTC ISO 8601), `value`, `unit`;
  Status-Topic `ok`, nach Abstecken automatisch `offline` (Last Will, retained).
- Refactoring: veränderliche Werte in Konfiguration, Logik in Klassen, Intervall aus Konfiguration.
- Hinweise: NTP für Zeitstempel, eindeutige Client-ID, `RuntimeError`/NaN des DHT11 abfangen.

## Termin 2 – Netzwerke (HTTP-API)

- Netzwerkkonfiguration (WLAN) über Konfig-Datei, die beim Start gelesen wird.
- HTTP-Endpunkte (unverschlüsselt, JSON):
  - `/config` GET + POST (z.B. Sendeintervall)
  - `/status` GET (z.B. letzte Werte, Verbindungsstatus, Uptime)
- In `openapi.yaml` dokumentiert (bisher nur `/config`). Felder von `/status` und weitere `/config`-Felder
  in der Gesamtgruppe per PR gegen Upstream abstimmen (Frist: vor dem 3. Termin).
- Fehlerfälle: 404 (Pfad), 405 (Methode), 400 (kaputtes JSON) – Server läuft weiter.
- **Testfälle für die HTTP-API** im Repo (z.B. `tests/`), Ausführung dokumentiert. Abstufung:
  - REST Client (VS Code) → *befriedigend*
  - `unittest`/`pytest` → *gut*
  - OpenAPI-Tooling (z.B. **Schemathesis**, Swagger Codegen) → *sehr gut*
- Doku-Absatz für die **IT-Abteilung**: ausgehende Verbindungen (Broker `158.180.44.197:1883/TCP`,
  ggf. NTP `UDP 123`, DNS), eingehende (HTTP Port 80 nur im LAN), Whitelist; Fernzugriff auf die
  HTTP-API diskutieren (Gefahren, Hindernisse: NAT, Firewall, keine Auth/TLS, VPN/Reverse-Tunnel …).

## Termin 3 – Cloud (Container)

Containerisierte Datenhaltung + Visualisierung, ressourcenschonend, auf Laptop lauffähig.

Voraussetzungen (jede Stufe):
- Start mit minimalem Aufwand (`docker compose up`).
- Doku: Mikrocontroller per Konfiguration ins WLAN einbinden **und** Container-Lösung starten/nutzen.

| Stufe | Anforderung |
|---|---|
| genügend | Live-Daten des eigenen Sensors vom Broker lesen, speichern (CSV reicht), visualisieren |
| befriedigend | Echte Datenbank (InfluxDB, SQLite, DuckDB …) |
| gut | Neue Sensoren automatisch aufnehmen (`iiot/group/+/sensor/temperature/state`) |
| sehr gut | Zusätzlich **Übersicht über verbundene Sensoren** (Anzahl, letzte Verbindung, Status aus `iiot/group/+/status`) |
| Bonus | Kennzahlen Mittelwert/Min/Max – kann Schwächen ausgleichen |

## Bewertungskriterien (gesamt)

- **Nutzerfreundlichkeit & Doku:** Einrichtung für neue Nutzer, Klarheit/Vollständigkeit/Struktur,
  funktionierende Testfälle.
- **Implementierungsqualität:** Lesbarkeit, Wartbarkeit, Modularität; REST + OpenAPI-Nutzung;
  Integration Firmware ↔ Datenbank ↔ Analyse.
- **Implementierungsumfang:** Erfüllung der Anforderungen inkl. Abstufungen.

## Vorgegebene Doku-Struktur (`docs/Dokumentation.qmd`)

1. Zusammenfassung der Aufgabenstellung und Ziele
2. Anleitung für User – Konfiguration und Start des Mikrocontrollers; Empfehlungen für die IT-Abteilung
3. Anleitung für Developer – pdm/Quarto, Struktur des Projekts (Vorlage nennt `src/cloud` und `tests/`;
   bei uns liegt der Stack in `infrastructure/` → Struktur entsprechend anpassen)
