import { Component, EventEmitter, Output, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { ApiService } from '../../services/api.service';

@Component({
  selector: 'app-cloud-discover',
  standalone: true,
  imports: [CommonModule, FormsModule],
  template: `
    <div class="modal-overlay" (click)="onOverlayClick($event)">
      <div class="modal cloud-modal">
        <h2>Tuya Cloud Discovery</h2>

        @if (step() === 'creds') {
          <p class="desc">Enter your Tuya IoT Platform credentials to discover devices.</p>

          <div class="form-group">
            <label>API Key (Access ID)</label>
            <input type="text" [(ngModel)]="creds.api_key" placeholder="Your Tuya API Key">
          </div>

          <div class="form-group">
            <label>API Secret (Access Secret)</label>
            <input type="password" [(ngModel)]="creds.api_secret" placeholder="Your Tuya API Secret">
          </div>

          <div class="form-group">
            <label>Region</label>
            <select [(ngModel)]="creds.region">
              <option value="us">US (Western America)</option>
              <option value="eu">EU (Central Europe)</option>
              <option value="cn">CN (China)</option>
              <option value="in">IN (India)</option>
            </select>
          </div>

          @if (error()) {
            <div class="error-msg">{{ error() }}</div>
          }

          <div class="button-row">
            <button class="btn btn-secondary" (click)="closed.emit()">Cancel</button>
            <button class="btn btn-primary" (click)="discover()" [disabled]="loading()">
              {{ loading() ? 'Discovering...' : 'Discover' }}
            </button>
          </div>
        }

        @if (step() === 'results') {
          <p class="desc">Found {{ discoveredDevices().length }} device(s). Select devices to import.</p>

          <table>
            <thead>
              <tr>
                <th><input type="checkbox" [checked]="allSelected()" (change)="toggleAll()"></th>
                <th>Name</th>
                <th>Category</th>
                <th>Online</th>
                <th>Local Key</th>
              </tr>
            </thead>
            <tbody>
              @for (d of discoveredDevices(); track d.id) {
                <tr>
                  <td><input type="checkbox" [(ngModel)]="d.selected"></td>
                  <td>{{ d.name }}</td>
                  <td class="muted">{{ d.category }}</td>
                  <td>
                    <span class="dot" [class.dot-online]="d.online" [class.dot-offline]="!d.online"></span>
                  </td>
                  <td class="muted">{{ d.local_key | slice:0:4 }}****</td>
                </tr>
              } @empty {
                <tr>
                  <td colspan="5" class="empty">No devices found in your Tuya Cloud account.</td>
                </tr>
              }
            </tbody>
          </table>

          @if (error()) {
            <div class="error-msg">{{ error() }}</div>
          }

          @if (successMsg()) {
            <div class="success-msg">{{ successMsg() }}</div>
          }

          <div class="button-row">
            <button class="btn btn-secondary" (click)="step.set('creds')">Back</button>
            <div class="spacer"></div>
            <button class="btn btn-secondary" (click)="closed.emit()">Close</button>
            <button class="btn btn-primary" (click)="importSelected()" [disabled]="loading() || selectedCount() === 0">
              {{ loading() ? 'Importing...' : 'Import Selected (' + selectedCount() + ')' }}
            </button>
          </div>
        }
      </div>
    </div>
  `,
  styles: [`
    .cloud-modal { max-width: 600px; }
    .desc { color: var(--text-muted); margin-bottom: 16px; font-size: 14px; }
    .button-row { display: flex; gap: 8px; margin-top: 20px; align-items: center; }
    .spacer { flex: 1; }
    .muted { color: var(--text-muted); }
    .empty { text-align: center; color: var(--text-muted); padding: 24px 0 !important; }
    .error-msg {
      background: rgba(239, 83, 80, 0.15);
      border: 1px solid var(--error);
      color: var(--error);
      padding: 8px 12px;
      border-radius: 4px;
      font-size: 13px;
      margin-top: 8px;
    }
    .success-msg {
      background: rgba(76, 175, 80, 0.15);
      border: 1px solid var(--success);
      color: var(--success);
      padding: 8px 12px;
      border-radius: 4px;
      font-size: 13px;
      margin-top: 8px;
    }
    input[type="checkbox"] {
      width: 18px;
      height: 18px;
      accent-color: var(--accent);
    }
  `]
})
export class CloudDiscoverComponent {
  @Output() closed = new EventEmitter<void>();
  @Output() imported = new EventEmitter<void>();

  private api = inject(ApiService);

  step = signal<'creds' | 'results'>('creds');
  loading = signal(false);
  error = signal('');
  successMsg = signal('');
  discoveredDevices = signal<any[]>([]);

  creds = {
    api_key: '',
    api_secret: '',
    region: 'us'
  };

  onOverlayClick(event: MouseEvent) {
    if ((event.target as HTMLElement).classList.contains('modal-overlay')) {
      this.closed.emit();
    }
  }

  allSelected(): boolean {
    const devices = this.discoveredDevices();
    return devices.length > 0 && devices.every(d => d.selected);
  }

  toggleAll() {
    const newVal = !this.allSelected();
    this.discoveredDevices().forEach(d => d.selected = newVal);
  }

  selectedCount(): number {
    return this.discoveredDevices().filter(d => d.selected).length;
  }

  discover() {
    if (!this.creds.api_key || !this.creds.api_secret) {
      this.error.set('API Key and API Secret are required.');
      return;
    }

    this.loading.set(true);
    this.error.set('');

    this.api.cloudDiscover(this.creds).subscribe({
      next: (devices: any) => {
        const list = Array.isArray(devices) ? devices : devices.devices || [];
        this.discoveredDevices.set(list.map((d: any) => ({ ...d, selected: true })));
        this.step.set('results');
        this.loading.set(false);
      },
      error: (err) => {
        this.loading.set(false);
        this.error.set(err.error?.error || 'Discovery failed. Check your credentials.');
      }
    });
  }

  importSelected() {
    const selected = this.discoveredDevices().filter(d => d.selected);
    if (selected.length === 0) return;

    this.loading.set(true);
    this.error.set('');
    this.successMsg.set('');

    this.api.cloudImport(selected).subscribe({
      next: (result: any) => {
        this.loading.set(false);
        const count = result?.imported || selected.length;
        this.successMsg.set(`Successfully imported ${count} device(s).`);
        setTimeout(() => this.imported.emit(), 1500);
      },
      error: (err) => {
        this.loading.set(false);
        this.error.set(err.error?.error || 'Import failed.');
      }
    });
  }
}
