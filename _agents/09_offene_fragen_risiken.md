# 09 – Offene Entscheidungen und Risiken

## Entscheidungen (Team)

| ID | Frage | Optionen | Empfehlung |
|---|---|---|---|
| F1 | WLAN-Konfiguration vor Ort ohne Neu-Flashen? | a) so lassen (`.env`, Compile-Zeit) + gut dokumentieren · b) WiFiManager-Captive-Portal (AP-Modus beim ersten Start) · c) `settings`-Datei auf LittleFS | Zeitlich: a) + Doku; Storyline spräche für b) – nur wenn Zeit bleibt |
| F2 | Status-Payload Format | `{"status":"ok"}` (Schema) vs. Klartext `ok` | JSON-Objekt laut AsyncAPI; Collector versteht beides |
| F3 | Gruppenname exakt | `kloepfer-brose-hirschberg-kal` – ist „kal“ der vollständige Nachname des 4. Mitglieds? | im Team klären, sonst so lassen (nur lowercase) |
| F4 | Default-Sendeintervall | 5 s (Ist) vs. 60 s (Spec-Beispiel) | 10–60 s; per `/config` änderbar |
| F5 | Altdaten mit Mixed-Case-Gruppe | per Migration lowercasen vs. belassen | in Migration v2 lowercasen (eine Gruppe im Dashboard) |
| F6 | Restart-Policy | `on-failure:5` (dokumentiert) vs. `unless-stopped` | `unless-stopped` für Dauerbetrieb, sonst bewusst dokumentieren |
| F7 | Branch `origin/esp32_code` | löschen vs. mergen | Inhalt ist älter als `main` → löschen nach Rücksprache mit Carl |
| F8 | HTTP-API-Tests: Tooling | pytest (*gut*) + Schemathesis (*sehr gut*) | beides; pytest für gezielte Fälle, Schemathesis für Spec-Konformität |
| F9 | `/status`-Felder | eigene vs. Upstream-Gruppenvorschlag | Upstream prüfen; falls nichts gemergt: unser Schema (06) per PR vorschlagen |

## Risiken

| Risiko | Auswirkung | Gegenmaßnahme |
|---|---|---|
| Deadline 27.11. (≈2,5 Tage) | Unvollständige Abgabe | Doku (AP6) früh starten, Prioritäten der Roadmap einhalten |
| Hardware nur bei einer Person | AP1/AP2-Tests blockiert | Firmware-Änderungen bündeln, Tests mit `DEVICE_URL` skipbar |
| Broker-Passwort im Klartext / Geheimnisse | Leak im Repo | nur `.env` (gitignored); vor Commit `git diff --cached` prüfen |
| Fremde Gruppen senden unerwartete Formate | Lücken im Dashboard | Rohdaten bleiben in `messages`; AP3 erhöht Toleranz |
| Schemathesis-POST ändert Gerätekonfig | Gerät sendet in falschem Intervall | Fixture setzt Konfiguration nach Testlauf zurück |
| WAL + Grafana ro nach Restore | Grafana-Fehler `readonly database` | AP5-Test, Fallback `journal_mode=DELETE` |
| Firmware-Wechsel auf lowercase | Zwei Gruppen im Dashboard | AP3 lowercase + Migration |
| ESP32 verliert NTP | falsche Zeitstempel | Collector-Regel >300 s → Empfangszeit (AP3) |
| GitHub Pages im Fork nicht aktiv | Doku-Website fehlt | Settings → Pages → Branch `gh-pages` prüfen |
