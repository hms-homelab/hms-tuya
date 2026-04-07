import { Component, inject, signal, OnInit, OnDestroy } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ApiService } from '../../services/api.service';
import { DeviceFormComponent } from '../device-form/device-form.component';
import { CloudDiscoverComponent } from '../cloud-discover/cloud-discover.component';

@Component({
  selector: 'app-device-list',
  standalone: true,
  imports: [CommonModule, DeviceFormComponent, CloudDiscoverComponent],
  template: `
    <div class="card">
      <div class="header">
        <h1>Devices</h1>
        <div class="actions">
          <button class="btn btn-secondary" (click)="showCloudDiscover.set(true)">Discover from Cloud</button>
          <button class="btn btn-primary" (click)="openAdd()">Add Device</button>
        </div>
      </div>

      <table>
        <thead>
          <tr>
            <th>Status</th>
            <th>Name</th>
            <th>IP</th>
            <th>Type</th>
            <th>Version</th>
            <th>Power</th>
          </tr>
        </thead>
        <tbody>
          @for (device of devices(); track device.name) {
            <tr>
              <td>
                <span class="dot"
                  [class.dot-online]="device.enabled && device.online"
                  [class.dot-offline]="device.enabled && !device.online"
                  [class.dot-disabled]="!device.enabled">
                </span>
              </td>
              <td>
                <a class="device-name" (click)="openEdit(device)">{{ device.friendly_name || device.name }}</a>
              </td>
              <td class="muted">{{ device.ip }}</td>
              <td class="muted">{{ device.type }}</td>
              <td class="muted">{{ device.version }}</td>
              <td>
                @if (device.enabled) {
                  <label class="toggle">
                    <input type="checkbox"
                      [checked]="device.power_on"
                      (change)="togglePower(device)">
                    <span class="slider"></span>
                  </label>
                } @else {
                  <span class="muted">--</span>
                }
              </td>
            </tr>
          } @empty {
            <tr>
              <td colspan="6" class="empty">No devices configured. Add a device or discover from Tuya Cloud.</td>
            </tr>
          }
        </tbody>
      </table>
    </div>

    @if (showForm()) {
      <app-device-form
        [device]="editDevice()"
        (saved)="onFormSaved()"
        (cancelled)="closeForm()"
        (deleted)="onFormDeleted()">
      </app-device-form>
    }

    @if (showCloudDiscover()) {
      <app-cloud-discover
        (closed)="showCloudDiscover.set(false)"
        (imported)="onCloudImported()">
      </app-cloud-discover>
    }
  `,
  styles: [`
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 20px;
    }
    h1 { font-size: 22px; font-weight: 600; }
    .actions { display: flex; gap: 8px; }
    .device-name {
      color: var(--accent);
      cursor: pointer;
      text-decoration: none;
    }
    .device-name:hover { text-decoration: underline; }
    .muted { color: var(--text-muted); }
    .empty {
      text-align: center;
      color: var(--text-muted);
      padding: 40px 0 !important;
    }
  `]
})
export class DeviceListComponent implements OnInit, OnDestroy {
  private api = inject(ApiService);
  private pollTimer: ReturnType<typeof setInterval> | null = null;

  devices = signal<any[]>([]);
  showForm = signal(false);
  editDevice = signal<any>(null);
  showCloudDiscover = signal(false);

  ngOnInit() {
    this.loadDevices();
    this.pollTimer = setInterval(() => this.loadDevices(), 10000);
  }

  ngOnDestroy() {
    if (this.pollTimer) clearInterval(this.pollTimer);
  }

  loadDevices() {
    this.api.getDevices().subscribe({
      next: (devices) => this.devices.set(devices),
      error: (err) => console.error('Failed to load devices:', err)
    });
  }

  openAdd() {
    this.editDevice.set(null);
    this.showForm.set(true);
  }

  openEdit(device: any) {
    this.editDevice.set({ ...device });
    this.showForm.set(true);
  }

  closeForm() {
    this.showForm.set(false);
    this.editDevice.set(null);
  }

  onFormSaved() {
    this.closeForm();
    this.loadDevices();
  }

  onFormDeleted() {
    this.closeForm();
    this.loadDevices();
  }

  onCloudImported() {
    this.showCloudDiscover.set(false);
    this.loadDevices();
  }

  togglePower(device: any) {
    const newState = device.power_on ? 'off' : 'on';
    this.api.sendCommand(device.name, { state: newState }).subscribe({
      next: () => {
        device.power_on = !device.power_on;
      },
      error: (err) => console.error('Failed to toggle power:', err)
    });
  }
}
