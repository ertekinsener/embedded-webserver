// The device's HTTP API (README.md, "API"). Every payload is a
// FlatBuffer; errors come back as a status code with a short text.

import {
  type BridgeSettingsT,
  decodeLoginResponse,
  decodeSystemInfo,
  decodeUartPorts,
  encodeBridgeSettings,
  encodeChangePasswordRequest,
  encodeLoginRequest,
  encodeUartSettings,
  kFlatBufferType,
  type LoginResponseT,
  type SystemInfoT,
  type UartPortT,
  type UartSettingsT,
} from './codec';
import { session } from './session';

export class ApiError extends Error {
  constructor(
    readonly status: number,
    message: string,
  ) {
    super(message);
  }
}

// Called when the device answers 401 to a request with a token: the
// session expired or the device rebooted.
let onSessionLost: () => void = () => {};

export function setSessionLostHandler(handler: () => void): void {
  onSessionLost = handler;
}

async function request(method: string, path: string, body?: Uint8Array): Promise<Uint8Array> {
  const headers: Record<string, string> = {};
  const token = session.token();
  if (token) {
    headers['Authorization'] = `Bearer ${token}`;
  }
  if (body) {
    headers['Content-Type'] = kFlatBufferType;
  }
  let response: Response;
  try {
    // The FlatBuffers builder returns a view of a plain ArrayBuffer.
    const init = body as Uint8Array<ArrayBuffer> | undefined;
    response = await fetch(path, { method, headers, body: init, cache: 'no-store' });
  } catch {
    throw new ApiError(0, 'The device is not reachable');
  }
  if (!response.ok) {
    const reason = (await response.text()).trim() || response.statusText;
    if (response.status === 401 && token) {
      session.clear();
      onSessionLost();
    }
    throw new ApiError(response.status, reason);
  }
  return new Uint8Array(await response.arrayBuffer());
}

export const api = {
  async login(username: string, password: string): Promise<LoginResponseT> {
    return decodeLoginResponse(
      await request('POST', '/api/auth/login', encodeLoginRequest(username, password)),
    );
  },

  async logout(): Promise<void> {
    await request('POST', '/api/auth/logout');
  },

  async changePassword(current: string, next: string): Promise<void> {
    await request('POST', '/api/auth/password', encodeChangePasswordRequest(current, next));
  },

  async resetSettings(): Promise<void> {
    await request('POST', '/api/settings/reset');
  },

  async systemInfo(): Promise<SystemInfoT> {
    return decodeSystemInfo(await request('GET', '/api/system/info'));
  },

  async uartPorts(): Promise<UartPortT[]> {
    return decodeUartPorts(await request('GET', '/api/uart/ports'));
  },

  async applyUartSettings(settings: UartSettingsT): Promise<void> {
    await request('POST', '/api/uart/config', encodeUartSettings(settings));
  },

  async applyBridgeSettings(settings: BridgeSettingsT): Promise<void> {
    await request('POST', '/api/bridge/config', encodeBridgeSettings(settings));
  },
};
