import { ComponentFixture, TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { DOCUMENTATION_URL, Documentation, slugify } from './documentation';

describe('Documentation page', () => {
  let fixture: ComponentFixture<Documentation>;
  let http: HttpTestingController;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [Documentation], providers: [provideHttpClient(), provideHttpClientTesting()],
    }).compileComponents();
    fixture = TestBed.createComponent(Documentation);
    http = TestBed.inject(HttpTestingController);
    fixture.detectChanges();
  });

  afterEach(() => { http.verify(); fixture.destroy(); });

  it('renders markdown with table of contents and external links in a new tab', () => {
    http.expectOne(DOCUMENTATION_URL).flush(
      '# Dokumentation\n\n## Anleitung für User\n\n### Container starten\n\nSiehe [README](https://example.org).\n\n| A | B |\n|---|---|\n| 1 | 2 |\n');
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;
    expect(root.querySelector('h2')?.id).toBe('anleitung-fur-user');
    const toc = [...root.querySelectorAll('nav[aria-label="Inhaltsverzeichnis"] a')].map(a => a.textContent?.trim());
    expect(toc).toEqual(['Anleitung für User', 'Container starten']);
    const link = root.querySelector('article a[href="https://example.org"]');
    expect(link?.getAttribute('target')).toBe('_blank');
    expect(root.querySelector('.table-wrap table')).not.toBeNull();
    expect(root.textContent).not.toContain('wird geladen');
  });

  it('shows an error if the documentation is missing', () => {
    http.expectOne(DOCUMENTATION_URL).flush('missing', { status: 404, statusText: 'Not Found' });
    fixture.detectChanges();
    expect(fixture.nativeElement.textContent).toContain('Dokumentation konnte nicht geladen werden.');
  });
});

describe('slugify', () => {
  it('builds stable heading ids', () => {
    expect(slugify('Empfehlungen für die IT-Abteilung')).toBe('empfehlungen-fur-die-it-abteilung');
    expect(slugify('Größe & Maße')).toBe('grosse-masse');
  });
});
