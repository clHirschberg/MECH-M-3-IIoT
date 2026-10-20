import { inject, Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';

@Injectable({ providedIn: 'root' })
export class EspApiService {
  private readonly http = inject(HttpClient);

  // Durch die IP-Adresse des ESP ersetzen.
  private readonly baseUrl = 'http://192.168.1.100';

  getStatus() {
    return this.http.get<{ status: string }>(`${this.baseUrl}/status`);
  }

  getInterval() {
    return this.http.get<{ interval: number }>(`${this.baseUrl}/interval`);
  }

  setInterval(interval: number) {
    return this.http.put(`${this.baseUrl}/interval`, { interval });
  }
}