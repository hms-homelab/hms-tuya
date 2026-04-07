import { Component } from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { NavBarComponent } from './components/nav-bar/nav-bar.component';
import { LogViewerComponent } from './components/log-viewer/log-viewer.component';

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [RouterOutlet, NavBarComponent, LogViewerComponent],
  template: `
    <app-nav-bar />
    <main class="content">
      <router-outlet />
    </main>
    <app-log-viewer />
  `,
  styles: [`
    .content {
      padding-bottom: 48px;
      min-height: calc(100vh - 56px);
    }
  `]
})
export class App {}
