import { Routes } from '@angular/router';
import { DeviceListComponent } from './components/device-list/device-list.component';
import { SettingsComponent } from './components/settings/settings.component';

export const routes: Routes = [
  { path: '', redirectTo: '/devices', pathMatch: 'full' },
  { path: 'devices', component: DeviceListComponent },
  { path: 'settings', component: SettingsComponent },
  { path: '**', redirectTo: '/devices' }
];
