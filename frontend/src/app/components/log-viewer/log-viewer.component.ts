import { Component, inject, signal, OnInit, OnDestroy, ElementRef, ViewChild } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { ApiService } from '../../services/api.service';

@Component({
  selector: 'app-log-viewer',
  standalone: true,
  imports: [CommonModule, FormsModule],
  template: `
    <div class="log-panel" [class.collapsed]="!expanded()">
      <div class="log-header" (click)="expanded.set(!expanded())">
        <span class="log-title">Logs</span>
        <div class="log-controls" (click)="$event.stopPropagation()">
          <select [(ngModel)]="deviceFilter" class="filter-select">
            <option value="">All Devices</option>
            @for (name of deviceNames(); track name) {
              <option [value]="name">{{ name }}</option>
            }
          </select>
          <button class="btn btn-secondary btn-sm" (click)="clearLogs()">Clear</button>
        </div>
        <span class="expand-icon">{{ expanded() ? '\u25BC' : '\u25B2' }}</span>
      </div>
      @if (expanded()) {
        <div class="log-body" #logBody>
          @for (log of filteredLogs(); track $index) {
            <div class="log-line" [class.log-warning]="log.level === 'WARNING'" [class.log-error]="log.level === 'ERROR'">
              <span class="log-ts">{{ log.timestamp }}</span>
              <span class="log-device">{{ log.device || '--' }}</span>
              <span class="log-msg">{{ log.message }}</span>
            </div>
          } @empty {
            <div class="log-line muted">No log entries.</div>
          }
        </div>
      }
    </div>
  `,
  styles: [`
    .log-panel {
      position: fixed;
      bottom: 0;
      left: 0;
      right: 0;
      background: var(--bg-card);
      border-top: 1px solid var(--border);
      z-index: 50;
      transition: height 0.2s;
    }
    .log-panel.collapsed {
      height: 40px;
    }
    .log-header {
      display: flex;
      align-items: center;
      padding: 8px 16px;
      cursor: pointer;
      user-select: none;
      border-bottom: 1px solid var(--border);
      height: 40px;
    }
    .log-title {
      font-size: 13px;
      font-weight: 600;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .log-controls {
      margin-left: auto;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .expand-icon {
      margin-left: 12px;
      color: var(--text-muted);
      font-size: 12px;
    }
    .filter-select {
      width: auto;
      min-width: 140px;
      padding: 4px 8px;
      font-size: 12px;
    }
    .btn-sm {
      padding: 4px 10px;
      font-size: 12px;
    }
    .log-body {
      height: 200px;
      overflow-y: auto;
      padding: 4px 0;
      font-family: 'SF Mono', 'Fira Code', 'Consolas', monospace;
      font-size: 12px;
      line-height: 1.6;
    }
    .log-line {
      padding: 1px 16px;
      display: flex;
      gap: 12px;
      white-space: nowrap;
    }
    .log-line:hover { background: var(--bg-hover); }
    .log-ts { color: var(--text-muted); min-width: 180px; }
    .log-device { color: var(--accent); min-width: 120px; }
    .log-msg { color: var(--text); overflow: hidden; text-overflow: ellipsis; }
    .log-warning .log-msg { color: var(--warning); }
    .log-error .log-msg { color: var(--error); }
    .muted { color: var(--text-muted); justify-content: center; padding: 16px; }
  `]
})
export class LogViewerComponent implements OnInit, OnDestroy {
  @ViewChild('logBody') logBody!: ElementRef;

  private api = inject(ApiService);
  private pollTimer: ReturnType<typeof setInterval> | null = null;

  expanded = signal(false);
  logs = signal<any[]>([]);
  deviceNames = signal<string[]>([]);
  deviceFilter = '';

  ngOnInit() {
    this.loadLogs();
    this.pollTimer = setInterval(() => this.loadLogs(), 5000);
  }

  ngOnDestroy() {
    if (this.pollTimer) clearInterval(this.pollTimer);
  }

  loadLogs() {
    this.api.getLogs().subscribe({
      next: (logs) => {
        this.logs.set(logs);
        const names = [...new Set(logs.map((l: any) => l.device).filter(Boolean))];
        this.deviceNames.set(names.sort());
      },
      error: () => {}
    });
  }

  filteredLogs(): any[] {
    const all = this.logs();
    if (!this.deviceFilter) return all;
    return all.filter(l => l.device === this.deviceFilter);
  }

  clearLogs() {
    this.logs.set([]);
  }
}
