import { Component, inject, signal, OnInit } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { ApiService } from '../../services/api.service';

@Component({
  selector: 'app-settings',
  standalone: true,
  imports: [CommonModule, FormsModule],
  template: `
    <div class="card">
      <h1>Settings</h1>

      @if (loading()) {
        <p class="muted">Loading settings...</p>
      } @else {
        <!-- MQTT Section -->
        <div class="section">
          <h3>MQTT</h3>
          <div class="form-row">
            <div class="form-group">
              <label>Broker</label>
              <input type="text" [(ngModel)]="settings.mqtt.broker" placeholder="192.168.2.15">
            </div>
            <div class="form-group narrow">
              <label>Port</label>
              <input type="number" [(ngModel)]="settings.mqtt.port" placeholder="1883">
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label>Username</label>
              <input type="text" [(ngModel)]="settings.mqtt.username">
            </div>
            <div class="form-group">
              <label>Password</label>
              <input type="password" [(ngModel)]="settings.mqtt.password">
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label>Client ID</label>
              <input type="text" [(ngModel)]="settings.mqtt.client_id" placeholder="hms-tuya">
            </div>
            <div class="form-group">
              <label>Topic Prefix</label>
              <input type="text" [(ngModel)]="settings.mqtt.topic_prefix" placeholder="tuya">
            </div>
          </div>
        </div>

        <!-- Tuya Cloud Section -->
        <div class="section">
          <h3>Tuya Cloud</h3>
          <div class="form-row">
            <div class="form-group">
              <label>API Key</label>
              <input type="text" [(ngModel)]="settings.cloud.api_key">
            </div>
            <div class="form-group">
              <label>API Secret</label>
              <input type="password" [(ngModel)]="settings.cloud.api_secret">
            </div>
          </div>
          <div class="form-group" style="max-width: 200px;">
            <label>Region</label>
            <select [(ngModel)]="settings.cloud.region">
              <option value="us">US</option>
              <option value="eu">EU</option>
              <option value="cn">CN</option>
              <option value="in">IN</option>
            </select>
          </div>
        </div>

        <!-- Tuya Tuning Section -->
        <div class="section">
          <h3>Tuya Tuning</h3>
          <div class="form-row">
            <div class="form-group">
              <label>Poll Interval (s)</label>
              <input type="number" [(ngModel)]="settings.tuning.poll_interval">
            </div>
            <div class="form-group">
              <label>Socket Timeout (s)</label>
              <input type="number" [(ngModel)]="settings.tuning.socket_timeout">
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label>Min Backoff (s)</label>
              <input type="number" [(ngModel)]="settings.tuning.min_backoff">
            </div>
            <div class="form-group">
              <label>Max Backoff (s)</label>
              <input type="number" [(ngModel)]="settings.tuning.max_backoff">
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label>Command Max Retries</label>
              <input type="number" [(ngModel)]="settings.tuning.cmd_max_retries">
            </div>
            <div class="form-group">
              <label>Command Retry Delay (ms)</label>
              <input type="number" [(ngModel)]="settings.tuning.cmd_retry_delay">
            </div>
          </div>
        </div>

        @if (error()) {
          <div class="error-msg">{{ error() }}</div>
        }

        @if (successMsg()) {
          <div class="success-msg">{{ successMsg() }}</div>
        }

        <div class="button-row">
          <button class="btn btn-primary" (click)="save()" [disabled]="saving()">
            {{ saving() ? 'Saving...' : 'Save Settings' }}
          </button>
        </div>
      }
    </div>
  `,
  styles: [`
    h1 { font-size: 22px; font-weight: 600; margin-bottom: 24px; }
    .section {
      margin-bottom: 32px;
      padding-bottom: 24px;
      border-bottom: 1px solid var(--border);
    }
    .section:last-of-type { border-bottom: none; }
    h3 {
      font-size: 15px;
      font-weight: 600;
      color: var(--accent);
      margin-bottom: 16px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .form-row { display: flex; gap: 16px; }
    .form-row .form-group { flex: 1; }
    .narrow { max-width: 120px; }
    .muted { color: var(--text-muted); }
    .button-row { margin-top: 24px; }
    .error-msg {
      background: rgba(239, 83, 80, 0.15);
      border: 1px solid var(--error);
      color: var(--error);
      padding: 8px 12px;
      border-radius: 4px;
      font-size: 13px;
      margin-top: 12px;
    }
    .success-msg {
      background: rgba(76, 175, 80, 0.15);
      border: 1px solid var(--success);
      color: var(--success);
      padding: 8px 12px;
      border-radius: 4px;
      font-size: 13px;
      margin-top: 12px;
    }
  `]
})
export class SettingsComponent implements OnInit {
  private api = inject(ApiService);

  loading = signal(true);
  saving = signal(false);
  error = signal('');
  successMsg = signal('');

  settings: any = {
    mqtt: { broker: '', port: 1883, username: '', password: '', client_id: '', topic_prefix: '' },
    cloud: { api_key: '', api_secret: '', region: 'us' },
    tuning: { poll_interval: 15, socket_timeout: 5, min_backoff: 5, max_backoff: 300, cmd_max_retries: 3, cmd_retry_delay: 500 }
  };

  ngOnInit() {
    this.api.getSettings().subscribe({
      next: (s: any) => {
        this.settings = {
          mqtt: { ...this.settings.mqtt, ...(s.mqtt || {}) },
          cloud: { ...this.settings.cloud, ...(s.cloud || {}) },
          tuning: { ...this.settings.tuning, ...(s.tuning || {}) }
        };
        this.loading.set(false);
      },
      error: () => {
        this.loading.set(false);
      }
    });
  }

  save() {
    this.saving.set(true);
    this.error.set('');
    this.successMsg.set('');

    this.api.updateSettings(this.settings).subscribe({
      next: () => {
        this.saving.set(false);
        this.successMsg.set('Settings saved successfully.');
        setTimeout(() => this.successMsg.set(''), 3000);
      },
      error: (err) => {
        this.saving.set(false);
        this.error.set(err.error?.error || 'Failed to save settings.');
      }
    });
  }
}
