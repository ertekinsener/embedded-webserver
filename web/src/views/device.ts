import { text } from '../api/codec';
import { formatIpv4, formatMac, formatUptime } from '../format';
import { h } from '../ui/dom';
import type { AppData } from './shell';

export function deviceView(data: AppData): HTMLElement {
  const { system } = data;
  return h(
    'div',
    { class: 'grid grid-cols-1 gap-6 lg:grid-cols-2' },
    section('Firmware', [
      ['Device', text(system.device)],
      ['Platform', text(system.platform)],
      ['Version', text(system.firmwareVersion)],
      ['Built', text(system.buildDate)],
      ['Uptime', formatUptime(system.uptimeS)],
    ]),
    section('Network', [
      ['IPv4 address', formatIpv4(system.ipv4Address)],
      ['Netmask', formatIpv4(system.ipv4Netmask)],
      ['MAC address', formatMac(system.macAddress)],
    ]),
  );
}

function section(title: string, rows: [string, string][]): HTMLElement {
  return h(
    'section',
    { class: 'card p-6' },
    h('h2', { class: 'card-title mb-4' }, title),
    h(
      'dl',
      { class: 'divide-y divide-slate-800' },
      ...rows.map(([label, value]) =>
        h(
          'div',
          { class: 'flex justify-between gap-4 py-2.5 text-sm' },
          h('dt', { class: 'text-slate-500' }, label),
          h('dd', { class: 'truncate text-right font-mono text-white' }, value || '-'),
        ),
      ),
    ),
  );
}
