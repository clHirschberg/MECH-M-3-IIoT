import { Routes } from '@angular/router';
import { Dashboard } from './frontend/dashboard';

export const routes: Routes = [
  { path: '', component: Dashboard },
  { path: 'messdaten', loadComponent: () => import('./measurements/measurements').then(m => m.Measurements) },
  { path: 'doku', loadComponent: () => import('./docs/documentation').then(m => m.Documentation) },
  { path: '**', redirectTo: '' },
];
