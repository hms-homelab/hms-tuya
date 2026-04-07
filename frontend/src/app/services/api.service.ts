import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';

@Injectable({ providedIn: 'root' })
export class ApiService {
  private http = inject(HttpClient);

  // Status
  getHealth() { return this.http.get('/health'); }
  getStatus() { return this.http.get('/api/status'); }

  // Devices
  getDevices() { return this.http.get<any[]>('/api/devices'); }
  addDevice(device: any) { return this.http.post('/api/devices', device); }
  updateDevice(name: string, device: any) { return this.http.put(`/api/devices/${name}`, device); }
  deleteDevice(name: string) { return this.http.delete(`/api/devices/${name}`); }
  getDeviceState(name: string) { return this.http.get(`/api/devices/${name}/state`); }
  sendCommand(name: string, cmd: any) { return this.http.post(`/api/devices/${name}/command`, cmd); }

  // Logs
  getLogs(count = 200) { return this.http.get<any[]>(`/api/logs?count=${count}`); }

  // Settings
  getSettings() { return this.http.get<any>('/api/settings'); }
  updateSettings(settings: any) { return this.http.put('/api/settings', settings); }

  // Cloud
  cloudDiscover(creds?: any) { return this.http.post<any[]>('/api/cloud/discover', creds || {}); }
  cloudImport(devices: any[]) { return this.http.post<any>('/api/cloud/import', { devices }); }
}
