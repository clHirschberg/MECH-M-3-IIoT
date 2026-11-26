import { afterNextRender, Component, DestroyRef, effect, ElementRef, inject, signal, ViewChild } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { Marked } from 'marked';
import { ThemeService } from '../theme.service';

export interface TocEntry { id: string; text: string; level: number; }

/** Pfad der Doku-Kopie (scripts/sync-docs.mjs erzeugt sie aus docs/Dokumentation.qmd). */
export const DOCUMENTATION_URL = 'docs/dokumentation.md';

const escapeHtml = (text: string) =>
  text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

const markdown = new Marked({ gfm: true });
markdown.use({
  renderer: {
    // Mermaid-Diagramme als Platzhalter ausgeben; gerendert wird nach dem Einfügen (lazy geladen)
    code({ text, lang }) {
      return lang === 'mermaid' ? `<pre class="mermaid not-prose">${escapeHtml(text)}</pre>` : false;
    },
  },
});

export function slugify(text: string): string {
  return text.toLowerCase().normalize('NFKD').replace(/[̀-ͯ]/g, '')
    .replace(/ß/g, 'ss').replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
}

/** Eigene Dokumentation (docs/Dokumentation.qmd) als Seite der Website. */
@Component({
  selector: 'app-documentation', standalone: true,
  template: `
    <main class="mx-auto flex max-w-7xl gap-8 px-4 py-6 sm:px-6">
      @if (toc().length) {
        <nav aria-label="Inhaltsverzeichnis" class="sticky top-6 hidden max-h-[calc(100vh-3rem)] w-64 shrink-0 self-start overflow-auto text-sm lg:block">
          <p class="mb-2 font-semibold text-slate-900 dark:text-slate-100">Inhalt</p>
          <ul class="space-y-1 border-l border-slate-200 dark:border-slate-700">
            @for (entry of toc(); track entry.id) {
              <li>
                <a [href]="'#' + entry.id" (click)="scrollTo($event, entry.id)"
                  [class]="entry.level === 3 ? 'pl-6 text-slate-500 dark:text-slate-400' : 'pl-3 text-slate-700 dark:text-slate-300'"
                  class="-ml-px block border-l border-transparent py-0.5 hover:border-blue-600 hover:text-blue-700 dark:hover:border-blue-400 dark:hover:text-blue-400">{{ entry.text }}</a>
              </li>
            }
          </ul>
        </nav>
      }
      <article class="min-w-0 flex-1 rounded-xl bg-white p-5 shadow sm:p-8 dark:bg-slate-900 dark:shadow-none dark:ring-1 dark:ring-slate-800">
        @if (error()) {
          <p role="alert" class="rounded-lg bg-red-50 p-3 text-sm text-red-700 dark:bg-red-950/50 dark:text-red-300">{{ error() }}</p>
        } @else if (loading()) {
          <p class="text-sm text-slate-500 dark:text-slate-400">Dokumentation wird geladen …</p>
        }
        <div #content class="prose prose-slate max-w-none dark:prose-invert prose-headings:scroll-mt-6 prose-a:text-blue-700 dark:prose-a:text-blue-400 prose-pre:bg-slate-900 dark:prose-pre:bg-slate-950 dark:prose-pre:ring-1 dark:prose-pre:ring-slate-800 prose-code:before:content-none prose-code:after:content-none prose-table:text-sm [&_.mermaid]:flex [&_.mermaid]:justify-center [&_.mermaid]:bg-transparent [&_.table-wrap]:overflow-x-auto"></div>
      </article>
    </main>`,
})
export class Documentation {
  @ViewChild('content', { static: true }) content!: ElementRef<HTMLElement>;
  private readonly http = inject(HttpClient);
  private readonly destroy = inject(DestroyRef);
  private readonly theme = inject(ThemeService);
  readonly toc = signal<TocEntry[]>([]);
  readonly loading = signal(true);
  readonly error = signal('');
  private readonly rendered = signal(false);

  constructor() {
    afterNextRender(() => this.load());
    effect(() => {
      const dark = this.theme.dark();
      if (this.rendered()) void this.renderDiagrams(dark);
    });
  }

  load() {
    this.http.get(DOCUMENTATION_URL, { responseType: 'text' }).pipe(takeUntilDestroyed(this.destroy)).subscribe({
      next: text => { this.show(text); this.loading.set(false); },
      error: () => { this.error.set('Dokumentation konnte nicht geladen werden.'); this.loading.set(false); },
    });
  }

  scrollTo(event: Event, id: string) {
    event.preventDefault(); // Fragment-Links würden wegen <base href="/"> auf die Startseite führen
    this.content.nativeElement.querySelector(`#${CSS.escape(id)}`)?.scrollIntoView({ behavior: 'smooth' });
  }

  private show(text: string) {
    const root = this.content.nativeElement;
    // Eigene, mit der Website ausgelieferte Datei → direkt einfügen (Angular-Sanitizer würde SVG/IDs entfernen)
    root.innerHTML = markdown.parse(text, { async: false });
    const used = new Set<string>();
    const toc: TocEntry[] = [];
    root.querySelectorAll<HTMLElement>('h1, h2, h3').forEach(heading => {
      let id = slugify(heading.textContent ?? '') || 'abschnitt';
      for (let n = 2; used.has(id); n++) id = `${slugify(heading.textContent ?? '')}-${n}`;
      used.add(id);
      heading.id = id;
      if (heading.tagName !== 'H1') toc.push({ id, text: heading.textContent ?? '', level: Number(heading.tagName[1]) });
    });
    root.querySelectorAll('a[href^="http"]').forEach(link => {
      link.setAttribute('target', '_blank');
      link.setAttribute('rel', 'noopener noreferrer');
    });
    root.querySelectorAll('table').forEach(table => {
      const wrap = document.createElement('div');
      wrap.className = 'table-wrap';
      table.replaceWith(wrap);
      wrap.appendChild(table);
    });
    root.querySelectorAll<HTMLElement>('.mermaid').forEach(node => node.dataset['source'] = node.textContent ?? '');
    this.toc.set(toc);
    this.rendered.set(true);
  }

  private async renderDiagrams(dark: boolean) {
    const nodes = [...this.content.nativeElement.querySelectorAll<HTMLElement>('.mermaid')];
    if (!nodes.length) return;
    try {
      const mermaid = (await import('mermaid')).default;
      mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', theme: dark ? 'dark' : 'default',
        fontFamily: 'ui-sans-serif, system-ui, sans-serif' });
      for (const node of nodes) {
        node.removeAttribute('data-processed');
        // Lange LR-Ketten würden auf Seitenbreite zu klein skaliert → auf der Website hochkant zeichnen
        node.textContent = (node.dataset['source'] ?? '').replace(/^(\s*flowchart\s+)LR\b/, '$1TD');
      }
      await mermaid.run({ nodes });
    } catch {
      // Diagramm bleibt als Quelltext lesbar
    }
  }
}
