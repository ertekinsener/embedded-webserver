import { ApiError, api } from '../api/client';
import {
  FlowControl,
  Oversampling,
  Parity,
  StopBits,
  text,
  UartCapabilitiesT,
  UartSettingsT,
  type UartPortT,
  WordLength,
} from '../api/codec';
import {
  flowControlLabels,
  kCommonBaudRates,
  oversamplingLabels,
  parityLabels,
  portName,
  settingsSummary,
  stopBitsLabels,
  wordLengthLabels,
} from '../format';
import { h } from '../ui/dom';
import { icon } from '../ui/icons';
import { showToast } from '../ui/toast';

// Settings form of one port. Only what the port reports in its
// capabilities is offered, so the same page fits every platform (e.g. no
// FIFO or pin swap switches where the driver cannot change them).
export function uartPortView(port: UartPortT, onApplied: () => Promise<void>): HTMLElement {
  const caps = port.capabilities ?? new UartCapabilitiesT();
  const current = port.settings ?? new UartSettingsT(port.id);

  const baudRate = h('input', {
    class: 'field font-mono',
    attrs: {
      id: 'baud_rate',
      type: 'number',
      inputmode: 'numeric',
      min: String(caps.minBaudRate),
      max: String(caps.maxBaudRate),
      step: '1',
      required: true,
      list: 'baud_rates',
    },
  });
  const baudRates = h(
    'datalist',
    { attrs: { id: 'baud_rates' } },
    ...kCommonBaudRates
      .filter((rate) => rate >= caps.minBaudRate && rate <= caps.maxBaudRate)
      .map((rate) => h('option', { attrs: { value: String(rate) } })),
  );
  const wordLength = enumSelect('word_length', caps.wordLengths, wordLengthLabels);
  const parity = enumSelect('parity', caps.parities, parityLabels);
  const stopBits = enumSelect('stop_bits', caps.stopBits, stopBitsLabels);
  const flowControl = enumSelect('flow_control', caps.flowControls, flowControlLabels);
  const oversampling = enumSelect('oversampling', caps.oversampling, oversamplingLabels);
  const fifo = checkbox('fifo_enabled');
  const swap = checkbox('tx_rx_swap');

  const load = (settings: UartSettingsT): void => {
    baudRate.value = String(settings.baudRate);
    wordLength.value = String(settings.wordLength);
    parity.value = String(settings.parity);
    stopBits.value = String(settings.stopBits);
    flowControl.value = String(settings.flowControl);
    oversampling.value = String(settings.oversampling);
    fifo.checked = settings.fifoEnabled;
    swap.checked = settings.txRxSwap;
  };
  load(current);

  const read = (): UartSettingsT =>
    new UartSettingsT(
      port.id,
      Number(baudRate.value),
      Number(wordLength.value) as WordLength,
      Number(stopBits.value) as StopBits,
      Number(parity.value) as Parity,
      Number(flowControl.value) as FlowControl,
      Number(oversampling.value) as Oversampling,
      caps.fifo && fifo.checked,
      caps.txRxSwap && swap.checked,
    );

  const submit = h(
    'button',
    { class: 'btn btn-primary', attrs: { type: 'submit' } },
    icon('send'),
    'Apply',
  );
  const revert = h(
    'button',
    {
      class: 'btn btn-secondary',
      attrs: { type: 'button' },
      on: { click: () => load(current) },
    },
    icon('revert'),
    'Revert',
  );

  const form = h(
    'form',
    {
      class: 'space-y-6',
      on: {
        submit: async (event) => {
          event.preventDefault();
          if (!form.reportValidity()) {
            return;
          }
          submit.disabled = true;
          submit.replaceChildren(icon('spinner', 'h-4 w-4 animate-spin'), 'Applying...');
          try {
            const settings = read();
            await api.applyUartSettings(settings);
            showToast('success', `${portName(port)} configured`, settingsSummary(settings));
            await onApplied();
          } catch (error) {
            const reason = error instanceof ApiError ? error.message : String(error);
            showToast('error', 'Not applied', reason);
          } finally {
            submit.disabled = false;
            submit.replaceChildren(icon('send'), 'Apply');
          }
        },
      },
    },
    h(
      'div',
      { class: 'grid grid-cols-1 gap-6 lg:grid-cols-2' },
      section(
        'Line',
        labelled(
          'Baud rate (bit/s)',
          'baud_rate',
          baudRate,
          `${caps.minBaudRate} - ${caps.maxBaudRate}`,
        ),
        baudRates,
        labelled('Data bits', 'word_length', wordLength),
        labelled('Parity', 'parity', parity),
        labelled('Stop bits', 'stop_bits', stopBits),
      ),
      section(
        'Hardware',
        labelled(
          'Flow control',
          'flow_control',
          flowControl,
          caps.flowControls.length <= 1 ? 'No RTS/CTS pins on this port' : '',
        ),
        labelled('Oversampling', 'oversampling', oversampling),
        caps.fifo && toggle(fifo, 'Hardware FIFO', 'Buffer characters in the UART FIFO.'),
        caps.txRxSwap && toggle(swap, 'Swap TX/RX pins', 'For crossed cables, no rewiring.'),
      ),
    ),
    h('div', { class: 'flex flex-wrap justify-end gap-3' }, revert, submit),
  );

  return h(
    'div',
    { class: 'space-y-6' },
    h(
      'div',
      { class: 'card flex flex-wrap items-center gap-4 p-5' },
      h(
        'div',
        { class: 'flex h-10 w-10 items-center justify-center rounded-xl bg-primary/15 text-primary' },
        icon('port', 'h-5 w-5'),
      ),
      h(
        'div',
        { class: 'min-w-0 flex-1' },
        h('p', { class: 'font-semibold text-white' }, portName(port)),
        h('p', { class: 'text-sm text-slate-500' }, text(port.description)),
      ),
      h(
        'div',
        { class: 'text-right' },
        h('p', { class: 'text-xs uppercase tracking-wider text-slate-500' }, 'Running with'),
        h('span', { class: 'badge mt-1' }, settingsSummary(current)),
      ),
    ),
    form,
  );
}

function enumSelect<E extends number>(
  id: string,
  supported: E[],
  labels: Record<E, string>,
): HTMLSelectElement {
  return h(
    'select',
    {
      class: 'field cursor-pointer',
      attrs: { id, disabled: supported.length <= 1 },
    },
    ...supported.map((value) =>
      h('option', { attrs: { value: String(value) } }, labels[value] ?? String(value)),
    ),
  );
}

function checkbox(id: string): HTMLInputElement {
  return h('input', { class: 'h-5 w-5 cursor-pointer', attrs: { id, type: 'checkbox' } });
}

function section(title: string, ...children: (Node | false)[]): HTMLElement {
  return h(
    'section',
    { class: 'card space-y-4 p-6' },
    h('h2', { class: 'card-title border-b border-slate-800 pb-2' }, title),
    ...children,
  );
}

function labelled(label: string, id: string, control: HTMLElement, hint = ''): HTMLElement {
  return h(
    'div',
    {},
    h('label', { class: 'field-label', attrs: { for: id } }, label),
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
