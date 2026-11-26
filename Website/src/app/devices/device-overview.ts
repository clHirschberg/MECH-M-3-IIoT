import { afterNextRender, Component, DestroyRef, inject, signal } from '@angular/core';
import { DatePipe, DecimalPipe } from '@angular/common';
import { HttpClient } from '@angular/common/http';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';

export interface DeviceInfo {
  group: string;
  status: 'ok' | 'error' | 'offline' | 'unknown';
  status_updated_at: number | null;
  last_seen: number | null;
  last_value: number | null;
  unit: string | null;
  active: boolean;
}

export interface DevicesResponse {
  total: number;
  active: number;
  active_seconds: number;
  devices: DeviceInfo[];
}

const STATUS_CLASSES: Record<DeviceInfo['status'], string> = {
  ok: 'bg-green-100 text-green-800',
  error: 'bg-red-100 text-red-800',
  offline: 'bg-red-100 text-red-800',
  unknown: 'bg-amber-100 text-amber-800',
};

/** Übersicht aller Sensoren (Gruppen) mit Status aus iiot/group/+/status und letzter Verbindung. */
@Component({
  selector: 'app-device-overview', standalone: true,
  imports: [DatePipe, DecimalPipe],
  templateUrl: './device-overview.html',
})
export class DeviceOverview {
  private readonly http = inject(HttpClient);
  private readonly destroy = inject(DestroyRef);
  readonly data = signal<DevicesResponse | null>(null);
  readonly error = signal('');
  private timer?: ReturnType<typeof setInterval>;

  constructor() {
    afterNextRender(() => {
      this.load();
      this.timer = setInterval(() => { if (!document.hidden) this.load(); }, 10000);
    });
    this.destroy.onDestroy(() => clearInterval(this.timer));
  }

  statusClass(status: DeviceInfo['status']) {
    return STATUS_CLASSES[status] ?? STATUS_CLASSES.unknown;
  }

  load() {
    this.http.get<DevicesResponse>('/api/devices').pipe(takeUntilDestroyed(this.destroy)).subscribe({
      next: data => { this.data.set(data); this.error.set(''); },
      error: () => this.error.set('Geräteübersicht konnte nicht geladen werden.'),
    });
  }
}
