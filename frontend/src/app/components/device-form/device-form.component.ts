import { Component, EventEmitter, Input, Output, inject, signal, OnInit } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { ApiService } from '../../services/api.service';

@Component({
  selector: 'app-device-form',
  standalone: true,
  imports: [CommonModule, FormsModule],
  template: `
    <div class="modal-overlay" (click)="onOverlayClick($event)">
      <div class="modal">
        <h2>{{ isEdit ? 'Edit Device' : 'Add Device' }}</h2>

        <div class="form-group">
          <label>Name</label>
          <input type="text" [(ngModel)]="form.name" placeholder="living_room_light"
            [disabled]="isEdit" pattern="[a-z0-9_]+">
          <span class="hint">Lowercase letters, numbers, underscores only</span>
        </div>

        <div class="form-group">
          <label>Friendly Name</label>
          <input type="text" [(ngModel)]="form.friendly_name" placeholder="Living Room Light">
        </div>

        <div class="form-group">
          <label>IP Address</label>
          <input type="text" [(ngModel)]="form.ip" placeholder="192.168.2.xxx">
        </div>

        <div class="form-group">
          <label>Device ID</label>
          <input type="text" [(ngModel)]="form.device_id" placeholder="From Tuya Cloud">
        </div>

        <div class="form-group">
          <label>Local Key</label>
          <input type="text" [(ngModel)]="form.local_key" placeholder="16-character key" maxlength="16">
          <span class="hint">16 characters from Tuya Cloud</span>
        </div>

        <div class="form-group">
          <label>MAC Address</label>
          <input type="text" [(ngModel)]="form.mac" placeholder="Optional">
        </div>

        <div class="form-row">
          <div class="form-group">
            <label>Version</label>
            <select [(ngModel)]="form.version">
              <option value="3.1">3.1</option>
              <option value="3.3">3.3</option>
              <option value="3.4">3.4</option>
            </select>
          </div>

          <div class="form-group">
            <label>Type</label>
            <select [(ngModel)]="form.type">
              <option value="bulb">Bulb</option>
              <option value="switch">Switch</option>
            </select>
          </div>
        </div>

        <div class="form-group checkbox-group">
          <label class="checkbox-label">
            <input type="checkbox" [(ngModel)]="form.enabled">
            <span>Enabled</span>
          </label>
        </div>

        @if (error()) {
          <div class="error-msg">{{ error() }}</div>
        }

        <div class="button-row">
          @if (isEdit) {
            <button class="btn btn-danger" (click)="confirmDelete()">Delete</button>
          }
          <div class="spacer"></div>
          <button class="btn btn-secondary" (click)="cancelled.emit()">Cancel</button>
          <button class="btn btn-primary" (click)="save()" [disabled]="saving()">
            {{ saving() ? 'Saving...' : 'Save' }}
          </button>
        </div>
      </div>
    </div>
  `,
  styles: [`
    .hint {
      font-size: 11px;
      color: var(--text-muted);
      margin-top: 4px;
      display: block;
    }
    .form-row {
      display: flex;
      gap: 16px;
    }
    .form-row .form-group { flex: 1; }
    .checkbox-group { margin: 8px 0; }
    .checkbox-label {
      display: flex;
      align-items: center;
      gap: 8px;
      cursor: pointer;
      color: var(--text) !important;
      font-size: 14px !important;
    }
    .checkbox-label input[type="checkbox"] {
      width: 18px;
      height: 18px;
      accent-color: var(--accent);
    }
    .button-row {
      display: flex;
      align-items: center;
      gap: 8px;
      margin-top: 24px;
    }
    .spacer { flex: 1; }
    .error-msg {
      background: rgba(239, 83, 80, 0.15);
      border: 1px solid var(--error);
      color: var(--error);
      padding: 8px 12px;
      border-radius: 4px;
      font-size: 13px;
      margin-top: 8px;
    }
  `]
})
export class DeviceFormComponent implements OnInit {
  @Input() device: any = null;
  @Output() saved = new EventEmitter<void>();
  @Output() cancelled = new EventEmitter<void>();
  @Output() deleted = new EventEmitter<void>();

  private api = inject(ApiService);

  saving = signal(false);
  error = signal('');

  isEdit = false;

  form: any = {
    name: '',
    friendly_name: '',
    ip: '',
    device_id: '',
    local_key: '',
    mac: '',
    version: '3.4',
    type: 'switch',
    enabled: true
  };

  ngOnInit() {
    if (this.device) {
      this.isEdit = true;
      this.form = { ...this.device };
    }
  }

  onOverlayClick(event: MouseEvent) {
    if ((event.target as HTMLElement).classList.contains('modal-overlay')) {
      this.cancelled.emit();
    }
  }

  save() {
    if (!this.form.name || !this.form.friendly_name || !this.form.ip || !this.form.device_id || !this.form.local_key) {
      this.error.set('Please fill in all required fields.');
      return;
    }
    if (!/^[a-z0-9_]+$/.test(this.form.name)) {
      this.error.set('Name must be lowercase letters, numbers, and underscores only.');
      return;
    }
    if (this.form.local_key.length !== 16) {
      this.error.set('Local Key must be exactly 16 characters.');
      return;
    }

    this.saving.set(true);
    this.error.set('');

    const obs = this.isEdit
      ? this.api.updateDevice(this.form.name, this.form)
      : this.api.addDevice(this.form);

    obs.subscribe({
      next: () => {
        this.saving.set(false);
        this.saved.emit();
      },
      error: (err) => {
        this.saving.set(false);
        this.error.set(err.error?.error || 'Failed to save device.');
      }
    });
  }

  confirmDelete() {
    if (confirm(`Delete device "${this.form.friendly_name || this.form.name}"? This cannot be undone.`)) {
      this.api.deleteDevice(this.form.name).subscribe({
        next: () => this.deleted.emit(),
        error: (err) => this.error.set(err.error?.error || 'Failed to delete device.')
      });
    }
  }
}
