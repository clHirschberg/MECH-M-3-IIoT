# 05 – Angular-Website (`Website/`)

## Stack

Angular 21 (standalone Components, Signals, `afterNextRender`), SSR-Gerüst vorhanden (`server.ts`,
`app.config.server.ts`), im Container aber als statisches CSR-Build über nginx (`index.csr.html`).
Tailwind 4 (PostCSS), Chart.js 4.6.1, Tests mit vitest + jsdom, Prettier. Node 22.12+.

## Routing (`app.routes.ts`)

| Pfad | Komponente | Tab |
|---|---|---|
| `/` | `frontend/dashboard.ts` (`Dashboard`) | **Geräte-API** |
| `/messdaten` | `measurements/measurements.ts` (lazy) | **Messdaten** |
| `/doku` | `docs/documentation.ts` (lazy) | **Dokumentation** (aus `docs/Dokumentation.qmd`, via `scripts/sync-docs.mjs`) |
| `**` | Redirect `/` | |

## Tab „Geräte-API“ (`frontend/dashboard.*`, `api/esp-api.service.ts`)

- IP-Eingabe in 4 Blöcken + Port (Default 80) → Basis-URL, in `localStorage` (`esp-api-base-url`).
- Aktionen: Status laden (`GET /status`, als JSON-Pipe angezeigt, Typ `unknown`),
  Intervall laden (`GET /config`), Intervall speichern (`POST /config {reading_interval_seconds}`).
- Validierung 1…86400 s. `device_id` ist im Modell (`EspConfig`), aber nicht editierbar.
- Browser → ESP32 direkt (CORS im Firmware-Code für Origin `localhost:4200`/`127.0.0.1:4200`).

Verbesserungsideen:
- `/status` typisieren (sobald OpenAPI-Schema steht) und als Karten statt Roh-JSON anzeigen.
- `device_id` editierbar machen.
- Optional: Client aus `openapi.yaml` generieren (passt zur Bewertung „Nutzung der OpenAPI Spec“).

## Tab „Messdaten“ (`measurements/*`)

- Lädt `/api/metadata`, `/api/config` (Grafana-Link) und `/api/readings`.
- Filter: Preset-Zeitraum (Sekunden) oder Custom Von/Bis, Gruppen, Sensoren, Auflösung (0 = auto),
  Min/Max-Band, Auto-Refresh 5/10/30 s (pausiert in verborgenem Tab), Reset.
- `measurement-chart.ts`: Chart.js-Linien mit Drag-Zoom (Pointer-Events), Reset-Button, Farben per Hash
  aus Gruppe/Sensor, Lücken bei fehlenden Buckets.
- Hinweis bei Empfangszeit statt Gerätezeit (`received_count`).

Möglicher Ausbau für *sehr gut* / iPhone-Storyline:
- Neuer Bereich/Tab **„Geräte“** mit Anzahl Sensoren, Status (ok/error/offline/unknown), letzter Verbindung,
  letztem Wert – basierend auf `GET /api/devices` (AP4). Grafana-Panel reicht formal aber aus.
- Mobile-Ansicht prüfen (QS-Leiter am iPhone).

## Darkmode

`theme.service.ts` (Hell/Dunkel/System, Umschalter in der Navigation), Tailwind-Variante `dark:` per Klasse
(`styles.css`), Chart-Farben folgen `ThemeService.dark()`.

## Tests

- `measurements.spec.ts`, `measurement-chart.spec.ts`, `app.spec.ts` (vitest): `cd Website && npm test`.

## Build / Dev

```bash
cd Website && npm install
ng serve -o            # Proxy /api → localhost:8000 (proxy.conf.json); Container "website" vorher stoppen
npm run build          # dist/Website/browser
```
