// Kopiert docs/Dokumentation.qmd als Markdown nach public/docs/, damit die Website die Doku anzeigt.
// Läuft automatisch vor `npm start`, `npm run build` und `npm test`. Im Docker-Build liegt docs/ nicht
// im Build-Kontext – dann wird die eingecheckte Kopie verwendet.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const source = resolve(here, '../../docs/Dokumentation.qmd');
const target = resolve(here, '../public/docs/dokumentation.md');
const repo = 'https://github.com/clHirschberg/MECH-M-3-IIoT/blob/main/docs/';

if (!existsSync(source)) {
  if (!existsSync(target)) throw new Error(`Weder ${source} noch ${target} vorhanden`);
  console.log('sync-docs: Quelle fehlt, eingecheckte Kopie wird verwendet');
  process.exit(0);
}

const markdown = readFileSync(source, 'utf8')
  .replace(/^```\{(\w+)\}/gm, '```$1') // Quarto-Codeblöcke (```{mermaid}) → normales Markdown
  .replace(/\]\((?!https?:|#|\/)([^)]+)\)/g, `](${repo}$1)`); // relative Links → GitHub

mkdirSync(dirname(target), { recursive: true });
const header = '<!-- Automatisch erzeugt aus docs/Dokumentation.qmd (scripts/sync-docs.mjs) – nicht bearbeiten -->\n';
const next = header + markdown;
if (!existsSync(target) || readFileSync(target, 'utf8') !== next) {
  writeFileSync(target, next);
  console.log('sync-docs: public/docs/dokumentation.md aktualisiert');
}
