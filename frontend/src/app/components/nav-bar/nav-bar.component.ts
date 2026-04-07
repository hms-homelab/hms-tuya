import { Component } from '@angular/core';
import { RouterLink, RouterLinkActive } from '@angular/router';

@Component({
  selector: 'app-nav-bar',
  standalone: true,
  imports: [RouterLink, RouterLinkActive],
  template: `
    <nav class="nav-bar">
      <div class="nav-brand">hms-tuya</div>
      <div class="nav-links">
        <a routerLink="/devices" routerLinkActive="active">Devices</a>
        <a routerLink="/settings" routerLinkActive="active">Settings</a>
      </div>
    </nav>
  `,
  styles: [`
    .nav-bar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 0 24px;
      height: 56px;
      background: var(--bg-card);
      border-bottom: 1px solid var(--border);
    }
    .nav-brand {
      font-size: 18px;
      font-weight: 700;
      color: var(--accent);
      letter-spacing: 0.5px;
    }
    .nav-links {
      display: flex;
      gap: 24px;
    }
    .nav-links a {
      color: var(--text-muted);
      text-decoration: none;
      font-size: 14px;
      padding: 8px 0;
      border-bottom: 2px solid transparent;
      transition: color 0.2s, border-color 0.2s;
    }
    .nav-links a:hover {
      color: var(--text);
    }
    .nav-links a.active {
      color: var(--accent);
      border-bottom-color: var(--accent);
    }
  `]
})
export class NavBarComponent {}
