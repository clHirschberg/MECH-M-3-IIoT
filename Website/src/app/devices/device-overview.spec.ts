import { ComponentFixture, TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { DeviceOverview, DevicesResponse } from './device-overview';

describe('Device overview', () => {
  let fixture: ComponentFixture<DeviceOverview>;
  let http: HttpTestingController;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [DeviceOverview], providers: [provideHttpClient(), provideHttpClientTesting()],
    }).compileComponents();
    fixture = TestBed.createComponent(DeviceOverview);
    http = TestBed.inject(HttpTestingController);
    fixture.detectChanges();
  });

  afterEach(() => { http.verify(); fixture.destroy(); });

  it('shows count, status and last connection per group', () => {
    const response: DevicesResponse = { total: 2, active: 1, active_seconds: 300, devices: [
      { group: 'a', status: 'ok', status_updated_at: 1, last_seen: Date.now(), last_value: 21.456, unit: '°C', active: true },
      { group: 'b', status: 'offline', status_updated_at: 1, last_seen: null, last_value: null, unit: null, active: false },
    ] };
    http.expectOne('/api/devices').flush(response);
    fixture.detectChanges();
    const text = fixture.nativeElement.textContent as string;
    expect(text).toContain('2 Sensoren');
    expect(text).toContain('1 aktiv');
    expect(text).toContain('21.46 °C');
    expect(text).toContain('noch keine Messwerte');
    const badges = fixture.nativeElement.querySelectorAll('tbody span');
    expect(badges[0].className).toContain('bg-green-100');
    expect(badges[1].className).toContain('bg-red-100');
  });

  it('shows an error message if the API is unavailable', () => {
    http.expectOne('/api/devices').flush('down', { status: 503, statusText: 'Service Unavailable' });
    fixture.detectChanges();
    expect(fixture.nativeElement.textContent).toContain('Geräteübersicht konnte nicht geladen werden.');
  });
});
