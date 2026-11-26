import { TestBed } from '@angular/core/testing';
import { ThemeService } from './theme.service';

describe('ThemeService', () => {
  afterEach(() => document.documentElement.classList.remove('dark'));

  it('toggles the dark class on <html> and remembers the choice', () => {
    const theme = TestBed.inject(ThemeService);
    theme.setMode('dark');
    TestBed.tick();
    expect(theme.dark()).toBe(true);
    expect(document.documentElement.classList.contains('dark')).toBe(true);
    theme.setMode('light');
    TestBed.tick();
    expect(document.documentElement.classList.contains('dark')).toBe(false);
  });
});
