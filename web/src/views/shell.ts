import { text, type SystemInfoT, type UartPortT } from '../api/codec';
import { portName, settingsSummary } from '../format';
import { h } from '../ui/dom';
import { icon, type IconName } from '../ui/icons';

export interface AppData {
  system: SystemInfoT;
  ports: UartPortT[];
}

export interface ShellActions {
  refresh: () => void;
  logout: () => void;
}

// The signed-in layout: a side menu (a drawer on small screens) and the
// page content. `active` is the current route, e.g. "#/ports/3".
export function shellView(
  data: AppData,
  active: string,
  title: string,
  content: HTMLElement,
  actions: ShellActions,
): HTMLElement {
  const backdrop = h('div', {
    class: 'fixed inset-0 z-30 hidden bg-black/60 md:hidden',
  });
  const sidebar = h(
    'aside',
    {
      class:
        'fixed inset-y-0 left-0 z-40 flex w-72 -translate-x-full flex-col border-r border-slate-800 ' +
        'bg-panel transition-transform md:static md:translate-x-0',
    },
    h(
      'div',
      { class: 'flex items-center gap-3 border-b border-slate-800 px-5 py-5' },
      h(
        'div',
        { class: 'flex h-10 w-10 items-center justify-center rounded-xl bg-primary/20 text-primary' },
        icon('chip', 'h-5 w-5'),
      ),
      h(
        'div',
        { class: 'min-w-0' },
        h('p', { class: 'font-bold text-white' }, 'UART Bridge'),
        h('p', { class: 'truncate font-mono text-xs text-slate-500' }, text(data.system.device)),
      ),
    ),
    h(
      'nav',
      { class: 'flex-1 space-y-6 overflow-y-auto px-3 py-5' },
      h('div', { class: 'space-y-1' }, navLink('#/overview', 'Overview', 'overview', active)),
      h(
        'div',
        { class: 'space-y-1' },
        h(
          'p',
          { class: 'px-3 pb-1 text-xs font-semibold uppercase tracking-wider text-slate-500' },
          'Settings',
        ),
        ...data.ports.map((port) =>
          navLink(`#/ports/${port.id}`, portName(port), 'port', active, settingsSummary(port.settings)),
        ),
        navLink('#/device', 'Device', 'device', active),
      ),
    ),
    h(
      'div',
      { class: 'border-t border-slate-800 p-3' },
      h(
        'button',
        { class: 'nav-link w-full', attrs: { type: 'button' }, on: { click: actions.logout } },
        icon('logout'),
        'Log out',
      ),
    ),
  );

  const setDrawer = (open: boolean): void => {
    sidebar.classList.toggle('-translate-x-full', !open);
    backdrop.classList.toggle('hidden', !open);
  };
  backdrop.addEventListener('click', () => setDrawer(false));
  sidebar.addEventListener('click', (event) => {
    if ((event.target as Element).closest('a')) {
      setDrawer(false);
    }
  });

  const header = h(
    'header',
    {
      class:
        'sticky top-0 z-20 flex items-center gap-3 border-b border-slate-800 bg-slate-950/90 px-4 py-3 backdrop-blur md:px-8',
    },
    h(
      'button',
      {
        class: 'btn btn-secondary px-3 md:hidden',
        attrs: { type: 'button', 'aria-label': 'Menu' },
        on: { click: () => setDrawer(true) },
      },
      icon('menu'),
    ),
    h('h1', { class: 'flex-1 truncate text-lg font-semibold text-white' }, title),
    h(
      'button',
      {
        class: 'btn btn-secondary px-3',
        attrs: { type: 'button', title: 'Reload from the device', 'aria-label': 'Reload' },
        on: { click: actions.refresh },
      },
      icon('refresh'),
    ),
  );

  return h(
    'div',
    { class: 'flex h-full' },
    backdrop,
    sidebar,
    h(
      'div',
      { class: 'flex min-w-0 flex-1 flex-col overflow-y-auto' },
      header,
      h('main', { class: 'mx-auto w-full max-w-5xl flex-1 p-4 md:p-8' }, content),
    ),
  );
}

function navLink(
  href: string,
  label: string,
  iconName: IconName,
  active: string,
  detail = '',
): HTMLAnchorElement {
  const isActive = active === href;
  return h(
    'a',
    {
      class: isActive ? 'nav-link nav-link-active' : 'nav-link',
      attrs: { href, 'aria-current': isActive ? 'page' : undefined },
    },
    icon(iconName),
    h('span', { class: 'flex-1 truncate' }, label),
    detail && h('span', { class: 'font-mono text-xs text-slate-500' }, detail),
  );
}
