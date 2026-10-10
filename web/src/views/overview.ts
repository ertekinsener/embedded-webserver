import { text } from '../api/codec';
import { bridgeSummary, formatIpv4, formatUptime, portName, settingsSummary } from '../format';
import { h } from '../ui/dom';
import { icon, type IconName } from '../ui/icons';
import type { AppData } from './shell';

export function overviewView(data: AppData): HTMLElement {
  const { system, ports } = data;
  return h(
    'div',
    { class: 'space-y-6' },
    h(
      'div',
      { class: 'grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-4' },
      statTile('device', 'Device', text(system.device)),
      statTile('chip', 'Platform', text(system.platform)),
      statTile('network', 'IP address', formatIpv4(system.ipv4Address)),
      statTile('clock', 'Uptime', formatUptime(system.uptimeS)),
    ),
    h(
      'section',
      { class: 'card overflow-hidden' },
      h(
        'div',
        { class: 'border-b border-slate-800 px-6 py-4' },
        h('h2', { class: 'card-title' }, 'UART ports'),
      ),
      ports.length === 0
        ? h('p', { class: 'px-6 py-8 text-sm text-slate-500' }, 'This device offers no UART ports.')
        : h(
            'ul',
            { class: 'divide-y divide-slate-800' },
            ...ports.map((port) =>
              h(
                'li',
                {},
                h(
                  'a',
                  {
                    class: 'flex items-center gap-4 px-6 py-4 transition hover:bg-slate-800/50',
                    attrs: { href: `#/ports/${port.id}` },
                  },
                  h(
                    'div',
                    {
                      class:
                        'flex h-9 w-9 shrink-0 items-center justify-center rounded-lg bg-slate-800 text-slate-400',
                    },
                    icon('port'),
                  ),
                  h(
                    'div',
                    { class: 'min-w-0 flex-1' },
                    h('p', { class: 'font-medium text-white' }, portName(port)),
                    h('p', { class: 'truncate text-xs text-slate-500' }, text(port.description)),
                  ),
                  bridgeSummary(port) &&
                    h(
                      'span',
                      {
                        class: port.bridgeStatus?.connected
                          ? 'badge hidden border-emerald-500/40 text-emerald-300 sm:inline-flex'
                          : 'badge hidden sm:inline-flex',
                      },
                      bridgeSummary(port),
                    ),
                  h('span', { class: 'badge' }, settingsSummary(port.settings)),
                  icon('chevron', 'h-3 w-3 text-slate-600'),
                ),
              ),
            ),
          ),
    ),
  );
}

function statTile(iconName: IconName, label: string, value: string): HTMLElement {
  return h(
    'div',
    { class: 'card flex items-center gap-4 p-5' },
    h(
      'div',
      { class: 'flex h-10 w-10 shrink-0 items-center justify-center rounded-xl bg-primary/15 text-primary' },
      icon(iconName, 'h-5 w-5'),
    ),
    h(
      'div',
      { class: 'min-w-0' },
      h('p', { class: 'text-xs uppercase tracking-wider text-slate-500' }, label),
      h('p', { class: 'truncate font-mono text-sm text-white', attrs: { title: value } }, value || '-'),
    ),
  );
}
