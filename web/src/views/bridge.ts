// "Ethernet bridge" card of a port page (docs/bridge/README.md): settings of
// the port's raw TCP server, its live status and how to connect to it.

import { ApiError, api } from '../api/client';
import { BridgeSettingsT, type BridgeStatusT, type UartPortT } from '../api/codec';
import { formatBytes, formatIpv4 } from '../format';
import { h, replaceChildren } from '../ui/dom';
import { icon } from '../ui/icons';
import { showToast } from '../ui/toast';

const kStatusRefreshMs = 2000;

export function bridgeCard(
  port: UartPortT,
  deviceAddress: string,
  onApplied: () => Promise<void>,
): HTMLElement | null {
  const settings = port.bridge;
  if (!settings) {
    return null;  // No bridge on this port (e.g. the console).
  }

  const enabled = h('input', {
    class: 'h-5 w-5 cursor-pointer',
    attrs: { id: 'bridge_enabled', type: 'checkbox' },
  });
  enabled.checked = settings.enabled;
  const tcpPort = numberField('bridge_tcp_port', settings.tcpPort, 1024, 65535);
  const idleGap = numberField('bridge_idle_gap', settings.idleGapMs, 0, 1000);
  const maxFrame = numberField('bridge_max_frame', settings.maxFrame, 1, 1460);

  const status = h('div', { class: 'space-y-3' });
  const showStatus = (current: BridgeStatusT | null, isEnabled: boolean): void => {
    replaceChildren(status, ...statusRows(current, isEnabled));
  };
  showStatus(port.bridgeStatus, settings.enabled);

  const submit = h(
    'button',
    { class: 'btn btn-primary', attrs: { type: 'submit' } },
    icon('send'),
    'Apply bridge',
  );
  const form = h(
    'form',
    {
      class: 'space-y-4',
      on: {
        submit: async (event) => {
          event.preventDefault();
          if (!form.reportValidity()) {
            return;
          }
          submit.disabled = true;
          try {
            await api.applyBridgeSettings(
              new BridgeSettingsT(
                port.id,
                enabled.checked,
                Number(tcpPort.value),
                Number(idleGap.value),
                Number(maxFrame.value),
              ),
            );
            showToast(
              'success',
              enabled.checked ? `Bridge on TCP ${tcpPort.value}` : 'Bridge off',
            );
            await onApplied();
          } catch (error) {
            showToast('error', 'Not applied', error instanceof ApiError ? error.message : String(error));
          } finally {
            submit.disabled = false;
          }
        },
      },
    },
    h(
      'label',
      {
        class:
          'flex cursor-pointer items-center gap-3 rounded-lg border border-slate-800 bg-slate-900/50 p-3 transition hover:bg-slate-800',
        attrs: { for: 'bridge_enabled' },
      },
      enabled,
      h(
        'div',
        { class: 'flex-1' },
        h('p', { class: 'text-sm font-medium text-white' }, 'Raw TCP server'),
        h('p', { class: 'text-xs text-slate-500' }, 'One client at a time; a new client replaces the old one.'),
      ),
    ),
    h(
      'div',
      { class: 'grid grid-cols-1 gap-4 sm:grid-cols-3' },
      labelled('TCP port', tcpPort, '1024 - 65535'),
      labelled('Idle gap (ms)', idleGap, '0: send when the line goes idle'),
      labelled('Max frame (bytes)', maxFrame, 'Per TCP segment'),
    ),
    h('div', { class: 'flex justify-end' }, submit),
  );

  const card = h(
    'section',
    { class: 'card space-y-5 p-6' },
    h(
      'div',
      { class: 'flex items-center gap-3 border-b border-slate-800 pb-2' },
      icon('network', 'h-4 w-4 text-primary'),
      h('h2', { class: 'card-title flex-1' }, 'Ethernet bridge'),
    ),
    h(
      'div',
      { class: 'grid grid-cols-1 gap-6 lg:grid-cols-2' },
      form,
      h(
        'div',
        { class: 'space-y-4' },
        status,
        settings.enabled && connectHint(deviceAddress, settings.tcpPort),
      ),
    ),
  );

  // Live status while the card is shown.
  const timer = setInterval(async () => {
    if (!card.isConnected) {
      clearInterval(timer);
      return;
    }
    try {
      const fresh = (await api.uartPorts()).find((p) => p.id === port.id);
      if (fresh?.bridge) {
        showStatus(fresh.bridgeStatus, fresh.bridge.enabled);
      }
    } catch {
      clearInterval(timer);  // Session lost or device gone: stop polling.
    }
  }, kStatusRefreshMs);
  return card;
}

function statusRows(status: BridgeStatusT | null, enabled: boolean): HTMLElement[] {
  const state = !enabled
    ? { text: 'Off', dot: 'bg-slate-500' }
    : status?.connected
      ? { text: `Connected: ${formatIpv4(status.clientAddress)}:${status.clientPort}`, dot: 'bg-emerald-500' }
      : status?.listening
        ? { text: 'Waiting for a client', dot: 'bg-amber-400' }
        : { text: 'Not listening (see the log)', dot: 'bg-red-500' };
  const counters: [string, string][] = [
    ['UART to network', formatBytes(status?.uartToNetBytes ?? 0)],
    ['Network to UART', formatBytes(status?.netToUartBytes ?? 0)],
    ['Dropped (no client)', formatBytes(status?.droppedBytes ?? 0)],
    ['UART errors', String(status?.uartErrors ?? 0)],
  ];
  return [
    h(
      'div',
      { class: 'flex items-center gap-2 text-sm text-white' },
      h('span', { class: `h-2.5 w-2.5 rounded-full ${state.dot}` }),
      state.text,
    ),
    h(
      'dl',
      { class: 'grid grid-cols-2 gap-x-4 gap-y-1 text-sm' },
      ...counters.flatMap(([label, value]) => [
        h('dt', { class: 'text-slate-500' }, label),
        h('dd', { class: 'text-right font-mono text-white' }, value),
      ]),
    ),
  ];
}

function connectHint(address: string, port: number): HTMLElement {
  const host = address === '-' ? '<device>' : address;
  return h(
    'div',
    { class: 'space-y-1 rounded-lg border border-slate-800 bg-slate-950/60 p-3' },
    h('p', { class: 'text-xs uppercase tracking-wider text-slate-500' }, 'Connect from a PC'),
    ...[
      `nc ${host} ${port}`,
      `socat pty,link=/tmp/ttyBridge,raw,echo=0 tcp:${host}:${port}`,
      `python -m serial.tools.miniterm socket://${host}:${port}`,
    ].map((line) => h('code', { class: 'block break-all font-mono text-xs text-slate-300' }, line)),
  );
}

function numberField(id: string, value: number, min: number, max: number): HTMLInputElement {
  const input = h('input', {
    class: 'field font-mono',
    attrs: {
      id,
      type: 'number',
      inputmode: 'numeric',
      min: String(min),
      max: String(max),
      step: '1',
      required: true,
    },
  });
  input.value = String(value);
  return input;
}

function labelled(label: string, control: HTMLInputElement, hint: string): HTMLElement {
  return h(
    'div',
    {},
    h('label', { class: 'field-label', attrs: { for: control.id } }, label),
    control,
    h('p', { class: 'field-hint' }, hint),
  );
}
