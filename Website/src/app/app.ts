import { Component, inject } from '@angular/core';
import { RouterLink, RouterLinkActive, RouterOutlet } from '@angular/router';
import { ThemeMode, ThemeService } from './theme.service';

@Component({
  selector: 'app-root', standalone: true,
  imports: [RouterLink, RouterLinkActive, RouterOutlet],
  templateUrl: './app.html',
})
export class App {
  readonly theme = inject(ThemeService);
  readonly themeOptions: { mode: ThemeMode; label: string }[] = [
    { mode: 'light', label: 'Hell' }, { mode: 'dark', label: 'Dunkel' }, { mode: 'system', label: 'System' },
  ];
}
