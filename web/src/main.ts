// Entry point: hash routes, session handling and data loading.
//
//   #/login       sign-in form (also shown whenever there is no session)
//   #/overview    device summary and port list
//   #/ports/<id>  settings of one UART port
//   #/device      firmware and network details

import { ApiError, api, setSessionLostHandler } from './api/client';
import { session } from './api/session';
import { formatIpv4, portName } from './format';
import { h, replaceChildren } from './ui/dom';
import { icon } from './ui/icons';
import { showToast } from './ui/toast';
import { deviceView } from './views/device';
import { loginView } from './views/login';
import { overviewView } from './views/overview';
import { type AppData, shellView } from './views/shell';
import { uartPortView } from './views/uart_port';

const root = document.getElementById('app') as HTMLElement;
let data: AppData | null = null;
let loginNotice = '';

async function loadData(): Promise<void> {
  const [system, ports] = await Promise.all([api.systemInfo(), api.uartPorts()]);
  data = { system, ports };
}

function navigate(hash: string): void {
  if (location.hash === hash) {
    void render();
  } else {
    location.hash = hash;
  }
}

function showLogin(): void {
  data = null;
  replaceChildren(
    root,
    loginView(() => {
      loginNotice = '';
      navigate('#/overview');
    }, loginNotice),
  );
}

async function logout(): Promise<void> {
  try {
    await api.logout();
  } catch {
    // The session is dropped locally either way.
  }
  session.clear();
  loginNotice = 'You have been logged out.';
  navigate('#/login');
}

function statusView(message: string, retry?: () => void): HTMLElement {
  return h(
    'div',
    { class: 'flex h-full flex-col items-center justify-center gap-4 p-8 text-slate-400' },
    retry ? icon('warning', 'h-8 w-8 text-red-400') : icon('spinner', 'h-8 w-8 animate-spin'),
    h('p', {}, message),
    retry &&
      h(
        'button',
        { class: 'btn btn-secondary', attrs: { type: 'button' }, on: { click: retry } },
        icon('refresh'),
        'Retry',
      ),
  );
}

async function render(): Promise<void> {
  const route = location.hash || '#/overview';
  if (!session.token() || route === '#/login') {
    if (session.token()) {
      navigate('#/overview');
      return;
    }
    showLogin();
    return;
  }

  if (!data) {
    replaceChildren(root, statusView('Loading...'));
    try {
      await loadData();
    } catch (error) {
      if (!session.token()) {
        return;  // 401: the session-lost handler shows the login.
      }
      const reason = error instanceof ApiError ? error.message : String(error);
      replaceChildren(root, statusView(reason, () => void render()));
      return;
    }
  }
  const current = data as AppData;

  const refresh = async (): Promise<void> => {
    try {
      await loadData();
      void render();
    } catch (error) {
      if (session.token()) {
        showToast('error', 'Reload failed', error instanceof ApiError ? error.message : String(error));
      }
    }
  };
  const actions = { refresh: () => void refresh(), logout: () => void logout() };

  const portMatch = /^#\/ports\/(\d+)$/.exec(route);
  if (portMatch) {
    const port = current.ports.find((p) => p.id === Number(portMatch[1]));
    if (port) {
      replaceChildren(
        root,
        shellView(
          current,
          route,
          `${portName(port)} settings`,
          uartPortView(port, formatIpv4(current.system.ipv4Address), refresh),
          actions,
        ),
      );
      return;
    }
  }
  if (route === '#/device') {
    replaceChildren(root, shellView(current, route, 'Device', deviceView(current), actions));
    return;
  }
  if (route !== '#/overview') {
    navigate('#/overview');
    return;
  }
  replaceChildren(root, shellView(current, route, 'Overview', overviewView(current), actions));
}

setSessionLostHandler(() => {
  loginNotice = 'Your session has expired. Please sign in again.';
  navigate('#/login');
});
window.addEventListener('hashchange', () => void render());
void render();
