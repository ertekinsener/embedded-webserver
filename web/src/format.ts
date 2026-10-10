import {
  FlowControl,
  Oversampling,
  Parity,
  StopBits,
  text,
  type UartPortT,
  type UartSettingsT,
  WordLength,
} from './api/codec';

export const wordLengthLabels: Record<WordLength, string> = {
  [WordLength.Bits_7]: '7 bits',
  [WordLength.Bits_8]: '8 bits',
  [WordLength.Bits_9]: '9 bits',
};

export const stopBitsLabels: Record<StopBits, string> = {
  [StopBits.Bits_0_5]: '0.5 bits',
  [StopBits.Bits_1]: '1 bit',
  [StopBits.Bits_1_5]: '1.5 bits',
  [StopBits.Bits_2]: '2 bits',
};

export const parityLabels: Record<Parity, string> = {
  [Parity.None]: 'None',
  [Parity.Even]: 'Even',
  [Parity.Odd]: 'Odd',
};

export const flowControlLabels: Record<FlowControl, string> = {
  [FlowControl.None]: 'None',
  [FlowControl.RTS]: 'RTS only',
  [FlowControl.CTS]: 'CTS only',
  [FlowControl.RTS_CTS]: 'RTS/CTS',
};

export const oversamplingLabels: Record<Oversampling, string> = {
  [Oversampling.Over16]: '16x',
  [Oversampling.Over8]: '8x',
};

const kDataBits: Record<WordLength, string> = {
  [WordLength.Bits_7]: '7',
  [WordLength.Bits_8]: '8',
  [WordLength.Bits_9]: '9',
};
const kParityLetters: Record<Parity, string> = {
  [Parity.None]: 'N',
  [Parity.Even]: 'E',
  [Parity.Odd]: 'O',
};
const kStopBits: Record<StopBits, string> = {
  [StopBits.Bits_0_5]: '0.5',
  [StopBits.Bits_1]: '1',
  [StopBits.Bits_1_5]: '1.5',
  [StopBits.Bits_2]: '2',
};

// "115200 8N1".
export function settingsSummary(settings: UartSettingsT | null): string {
  if (!settings) {
    return '-';
  }
  return (
    `${settings.baudRate} ${kDataBits[settings.wordLength]}` +
    `${kParityLetters[settings.parity]}${kStopBits[settings.stopBits]}`
  );
}

export function portName(port: UartPortT): string {
  return text(port.name) || `Port ${port.id}`;
}

// Standard rates shown as suggestions; any rate in the port's range works.
export const kCommonBaudRates = [
  1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1000000,
  2000000, 3000000, 4000000,
];

export function formatUptime(seconds: number): string {
  const days = Math.floor(seconds / 86400);
  const time = [Math.floor(seconds / 3600) % 24, Math.floor(seconds / 60) % 60, seconds % 60]
    .map((value) => String(value).padStart(2, '0'))
    .join(':');
  return days > 0 ? `${days}d ${time}` : time;
}

export function formatIpv4(bytes: number[]): string {
  return bytes.length === 4 ? bytes.join('.') : '-';
}

export function formatBytes(bytes: number): string {
  if (bytes < 1024) {
    return `${bytes} B`;
  }
  if (bytes < 1024 * 1024) {
    return `${(bytes / 1024).toFixed(1)} KiB`;
  }
  return `${(bytes / (1024 * 1024)).toFixed(1)} MiB`;
}

// "TCP 4001", "TCP 4001, connected" or "" for ports without bridge.
export function bridgeSummary(port: UartPortT): string {
  if (!port.bridge?.enabled) {
    return '';
  }
  return port.bridgeStatus?.connected
    ? `TCP ${port.bridge.tcpPort}, connected`
    : `TCP ${port.bridge.tcpPort}`;
}

export function formatMac(bytes: number[]): string {
  return bytes.length === 6
    ? bytes.map((b) => b.toString(16).padStart(2, '0')).join(':')
    : '-';
}
