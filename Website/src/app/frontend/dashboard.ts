import { Component, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { EspApiService } from '../api/esp-api.service';

@Component({
  selector: 'app-dashboard',
  standalone: true,
  imports: [FormsModule],
  templateUrl: './dashboard.html',
})
export class Dashboard {
  private readonly api = inject(EspApiService);

  readonly status = signal('Noch nicht abgefragt');
  readonly message = signal('');
  interval = 1000;

  getStatus() {
    this.message.set('Lädt …');

    this.api.getStatus().subscribe({
      next: (data) => {
        this.status.set(data.status);
        this.message.set('Status geladen.');
      },
      error: () => this.message.set('Status konnte nicht geladen werden.'),
    });
  }

  getInterval() {
    this.message.set('Lädt …');

    this.api.getInterval().subscribe({
      next: (data) => {
        this.interval = data.interval;
        this.message.set('Intervall geladen.');
      },
      error: () => this.message.set('Intervall konnte nicht geladen werden.'),
    });
  }

  saveInterval() {
    if (!Number.isInteger(this.interval) || this.interval < 1) {
      this.message.set('Bitte eine positive ganze Zahl eingeben.');
      return;
    }

    this.message.set('Speichert …');

    this.api.setInterval(this.interval).subscribe({
      next: () => this.message.set('Intervall gespeichert.'),
      error: () => this.message.set('Intervall konnte nicht gespeichert werden.'),
    });
  }
}