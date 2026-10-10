// "Ethernet bridge" card of a port page (docs/bridge/README.md): the mode
// and settings of the port's bridge, its live status and how to connect.

import { ApiError, api } from '../api/client';
import {
  BridgeMode,
  BridgeSettingsT,
  type BridgeStatusT,
  type UartPortT,
} from '../api/codec';
import { bridgeModeLabels, delimiterText, formatBytes, formatIpv4, parseDelimiter, parseIpv4 } from '../format';
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
  const mode = h(
    'select',
    { class: 'field cursor-pointer', attrs: { id: 'bridge_mode' } },
    ...(Object.entries(bridgeModeLabels) as [string, string][]).map(([value, label]) =>
      h('option', { attrs: { value } }, label),
    ),
  );
  mode.value = String(settings.mode);
  const localPort = numberField('bridge_local_port', settings.localPort || 4000 + port.id, 1024, 65535);
  const remoteAddress = h('input', {
    class: 'field font-mono',
    attrs: { id: 'bridge_remote_address', placeholder: '192.168.1.2', inputmode: 'decimal' },
  });
  const remoteAddressValue = formatIpv4(settings.remoteAddress);
  remoteAddress.value = remoteAddressValue === '0.0.0.0' || remoteAddressValue === '-' ? '' : remoteAddressValue;
  const remotePort = numberField('bridge_remote_port', settings.remotePort, 0, 65535);
  const idleGap = numberField('bridge_idle_gap', settings.idleGapMs, 0, 1000);
  const maxFrame = numberField('bridge_max_frame', settings.maxFrame, 1, 1460);
  const useDelimiter = h('input', {
    class: 'h-5 w-5 cursor-pointer',
    attrs: { id: 'bridge_use_delimiter', type: 'checkbox' },
  });
  useDelimiter.checked = settings.useDelimiter;
  const delimiter = h('input', {
    class: 'field w-24 font-mono',
    attrs: { id: 'bridge_delimiter', placeholder: '\\n' },
  });
  delimiter.value = delimiterText(settings.delimiter);

  const localPortField = labelled('Local port', localPort, '1024 - 65535');
  const remoteHint = h('p', { class: 'field-hint' });
  const remoteFields = h(
    'div',
    { class: 'grid grid-cols-1 gap-4 sm:grid-cols-2' },
    h(
      'div',
      {},
      h('label', { class: 'field-label', attrs: { for: remoteAddress.id } }, 'Remote address'),
      remoteAddress,
      remoteHint,
    ),
    labelled('Remote port', remotePort, ''),
  );
  const updateFields = (): void => {
    const current = Number(mode.value) as BridgeMode;
    localPortField.classList.toggle('hidden', current === BridgeMode.TcpClient);
    remoteFields.classList.toggle(
      'hidden',
      current !== BridgeMode.TcpClient && current !== BridgeMode.Udp,
    );
    remoteHint.textContent =
      current === BridgeMode.Udp ? 'Empty: answer the last sender' : 'The device connects here';
    delimiter.disabled = !useDelimiter.checked;
  };
  mode.addEventListener('change', updateFields);
  useDelimiter.addEventListener('change', updateFields);
  updateFields();

  const status = h('div', { class: 'space-y-3' });
  const showStatus = (current: UartPortT): void => {
    replaceChildren(status, ...statusRows(current));
  };
  showStatus(port);

  const submit = h(
    'button',
    { class: 'btn btn-primary', attrs: { type: 'submit' } },
    icon('send'),
    'Apply bridge',
  );
  const read = (): BridgeSettingsT | string => {
    const selected = Number(mode.value) as BridgeMode;
    const address = remoteAddress.value.trim() === '' ? [] : parseIpv4(remoteAddress.value);
    if (address === null) {
      return 'Remote address must look like 192.168.1.2';
    }
    const byte = parseDelimiter(delimiter.value);
    if (useDelimiter.checked && byte === null) {
      return 'Delimiter: one character, \\n, \\r, \\t or 0x00 - 0xFF';
    }
    if (selected === BridgeMode.TcpClient && (address.length === 0 || Number(remotePort.value) === 0)) {
      return 'TCP client mode needs a remote address and port';
    }
    return new BridgeSettingsT(
      port.id,
      enabled.checked,
      selected,
      Number(localPort.value),
      address.length === 4 ? address : [0, 0, 0, 0],
      Number(remotePort.value),
      Number(idleGap.value),
      Number(maxFrame.value),
      useDelimiter.checked,
      byte ?? 10,
    );
  };
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
          const next = read();
          if (typeof next === 'string') {
            showToast('error', 'Check the bridge settings', next);
            return;
          }
          submit.disabled = true;
          try {
            await api.applyBridgeSettings(next);
            showToast('success', next.enabled ? `Bridge on (${bridgeModeLabels[next.mode]})` : 'Bridge off');
            await onApplied();
          } catch (error) {
            showToast('error', 'Not applied', error instanceof ApiError ? error.message : String(error));
          } finally {
            submit.disabled = false;
          }
        },
      },
    },
    toggle(enabled, 'Bridge enabled', 'One client at a time; a new client replaces the old one.'),
    h(
      'div',
      { class: 'grid grid-cols-1 gap-4 sm:grid-cols-2' },
      labelled('Mode', mode, ''),
      localPortField,
    ),
    remoteFields,
    h(
      'div',
      { class: 'grid grid-cols-1 gap-4 sm:grid-cols-2' },
      labelled('Idle gap (ms)', idleGap, '0: send when the line goes idle'),
      labelled('Max frame (bytes)', maxFrame, 'Per TCP segment or datagram'),
    ),
    h(
      'div',
      { class: 'flex flex-wrap items-center gap-3' },
      h(
        'label',
        { class: 'flex cursor-pointer items-center gap-2 text-sm text-white', attrs: { for: useDelimiter.id } },
        useDelimiter,
        'Send at delimiter',
      ),
      delimiter,
      h('span', { class: 'field-hint mt-0' }, 'e.g. \\n or 0x0D'),
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
        settings.enabled && connectHint(deviceAddress, port),
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
        showStatus(fresh);
      }
    } catch {
      clearInterval(timer);  // Session lost or device gone: stop polling.
    }
  }, kStatusRefreshMs);
  return card;
}

function statusText(port: UartPortT, status: BridgeStatusT | null): { text: string; dot: string } {
  const settings = port.bridge;
  if (!settings?.enabled) {
    return { text: 'Off', dot: 'bg-slate-500' };
  }
  const peer = status ? `${formatIpv4(status.clientAddress)}:${status.clientPort}` : '';
  const remote = `${formatIpv4(settings.remoteAddress)}:${settings.remotePort}`;
  switch (settings.mode) {
    case BridgeMode.TcpClient:
      return status?.connected
        ? { text: `Connected to ${remote}`, dot: 'bg-emerald-500' }
        : { text: `Connecting to ${remote}`, dot: 'bg-amber-400' };
    case BridgeMode.Udp:
      if (status?.connected) {
        return { text: `Sending to ${peer}`, dot: 'bg-emerald-500' };
      }
      return status?.listening
        ? { text: 'Waiting for a datagram', dot: 'bg-amber-400' }
        : { text: 'Not bound (see the log)', dot: 'bg-red-500' };
    default:
      if (status?.connected) {
        return { text: `Connected: ${peer}`, dot: 'bg-emerald-500' };
      }
      return status?.listening
        ? { text: 'Waiting for a client', dot: 'bg-amber-400' }
        : { text: 'Not listening (see the log)', dot: 'bg-red-500' };
  }
}

function statusRows(port: UartPortT): HTMLElement[] {
  const status = port.bridgeStatus;
  const state = statusText(port, status);
  const counters: [string, string][] = [
    ['UART to network', formatBytes(status?.uartToNetBytes ?? 0)],
    ['Network to UART', formatBytes(status?.netToUartBytes ?? 0)],
    ['Dropped (no peer)', formatBytes(status?.droppedBytes ?? 0)],
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

function connectHint(address: string, port: UartPortT): HTMLElement {
  const host = address === '-' ? '<device>' : address;
  const settings = port.bridge as NonNullable<UartPortT['bridge']>;
  const local = settings.localPort;
  let lines: string[];
  switch (settings.mode) {
    case BridgeMode.Rfc2217:
      lines = [
        `python -m serial.tools.miniterm rfc2217://${host}:${local} 115200`,
        `serial.serial_for_url("rfc2217://${host}:${local}", baudrate=115200)`,
      ];
      break;
    case BridgeMode.TcpClient:
      lines = [`nc -lk ${settings.remotePort}    # on ${formatIpv4(settings.remoteAddress)}`];
      break;
    case BridgeMode.Udp:
      lines = [`nc -u ${host} ${local}`, `socat - UDP:${host}:${local}`];
      break;
    default:
      lines = [
        `nc ${host} ${local}`,
        `socat pty,link=/tmp/ttyBridge,raw,echo=0 tcp:${host}:${local}`,
        `python -m serial.tools.miniterm socket://${host}:${local}`,
      ];
      break;
  }
  return h(
    'div',
    { class: 'space-y-1 rounded-lg border border-slate-800 bg-slate-950/60 p-3' },
    h('p', { class: 'text-xs uppercase tracking-wider text-slate-500' }, 'Connect from a PC'),
    ...lines.map((line) => h('code', { class: 'block break-all font-mono text-xs text-slate-300' }, line)),
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

function labelled(label: string, control: HTMLInputElement | HTMLSelectElement, hint: string): HTMLElement {
  return h(
    'div',
    {},
    h('label', { class: 'field-label', attrs: { for: control.id } }, label),
    control,
    hint && h('p', { class: 'field-hint' }, hint),
  );
}

function toggle(input: HTMLInputElement, title: string, detail: string): HTMLElement {
  return h(
    'label',
    {
      class:
        'flex cursor-pointer items-center gap-3 rounded-lg border border-slate-800 bg-slate-900/50 p-3 transition hover:bg-slate-800',
      attrs: { for: input.id },
    },
    input,
    h(
      'div',
      { class: 'flex-1' },
      h('p', { class: 'text-sm font-medium text-white' }, title),
      h('p', { class: 'text-xs text-slate-500' }, detail),
    ),
  );
}
