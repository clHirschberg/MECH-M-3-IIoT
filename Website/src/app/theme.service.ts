import { DOCUMENT } from '@angular/common';
import { computed, effect, inject, Injectable, signal } from '@angular/core';

export type ThemeMode = 'system' | 'light' | 'dark';
const STORAGE_KEY = 'iiot-theme';

/** Hell/Dunkel-Modus: Auswahl in localStorage, „System“ folgt prefers-color-scheme. */
@Injectable({ providedIn: 'root' })
export class ThemeService {
  private readonly document = inject(DOCUMENT);
  private readonly media = this.document.defaultView?.matchMedia?.('(prefers-color-scheme: dark)');
  private readonly systemDark = signal(this.media?.matches ?? false);
  readonly mode = signal<ThemeMode>(this.readMode());
  readonly dark = computed(() => this.mode() === 'dark' || (this.mode() === 'system' && this.systemDark()));

  constructor() {
    this.media?.addEventListener?.('change', event => this.systemDark.set(event.matches));
    effect(() => {
      const dark = this.dark();
      this.document.documentElement.classList.toggle('dark', dark);
      this.document.documentElement.style.colorScheme = dark ? 'dark' : 'light';
    });
  }

  setMode(mode: ThemeMode) {
    this.mode.set(mode);
    try { this.document.defaultView?.localStorage.setItem(STORAGE_KEY, mode); } catch { /* z.B. privater Modus */ }
  }

  private readMode(): ThemeMode {
    try {
      const stored = this.document.defaultView?.localStorage.getItem(STORAGE_KEY);
      if (stored === 'light' || stored === 'dark' || stored === 'system') return stored;
    } catch { /* ignorieren */ }
    return 'system';
  }
}
