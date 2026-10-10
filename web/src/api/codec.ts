// FlatBuffers encoding and decoding of the API payloads, with the code flatc
// generates from webserver/schema/*.fbs (object API) and the FlatBuffers
// runtime. Independent of the DOM, so the smoke test can use it from Node.

import * as flatbuffers from 'flatbuffers';

import { BridgeSettingsT } from '@api/bridge-settings';
import { LoginRequestT } from '@api/login-request';
import { LoginResponse, LoginResponseT } from '@api/login-response';
import { SystemInfo, SystemInfoT } from '@api/system-info';
import { UartPortT } from '@api/uart-port';
import { UartPortList } from '@api/uart-port-list';
import { UartSettingsT } from '@api/uart-settings';

export { BridgeSettingsT } from '@api/bridge-settings';
export { BridgeStatusT } from '@api/bridge-status';
export { FlowControl } from '@api/flow-control';
export { Oversampling } from '@api/oversampling';
export { Parity } from '@api/parity';
export { StopBits } from '@api/stop-bits';
export { UartCapabilitiesT } from '@api/uart-capabilities';
export { UartSettingsT } from '@api/uart-settings';
export { WordLength } from '@api/word-length';
export type { LoginResponseT, SystemInfoT, UartPortT };

export const kFlatBufferType = 'application/x-flatbuffers';

interface Packable {
  pack(builder: flatbuffers.Builder): flatbuffers.Offset;
}

function finish(object: Packable): Uint8Array {
  const builder = new flatbuffers.Builder(256);
  builder.finish(object.pack(builder));
  return builder.asUint8Array();
}

export function text(value: string | Uint8Array | null): string {
  if (value === null) {
    return '';
  }
  return typeof value === 'string' ? value : new TextDecoder().decode(value);
}

export function encodeLoginRequest(username: string, password: string): Uint8Array {
  return finish(new LoginRequestT(username, password));
}

export function encodeUartSettings(settings: UartSettingsT): Uint8Array {
  return finish(settings);
}

export function encodeBridgeSettings(settings: BridgeSettingsT): Uint8Array {
  return finish(settings);
}

export function decodeLoginResponse(data: Uint8Array): LoginResponseT {
  return LoginResponse.getRootAsLoginResponse(new flatbuffers.ByteBuffer(data)).unpack();
}

export function decodeSystemInfo(data: Uint8Array): SystemInfoT {
  return SystemInfo.getRootAsSystemInfo(new flatbuffers.ByteBuffer(data)).unpack();
}

export function decodeUartPorts(data: Uint8Array): UartPortT[] {
  return UartPortList.getRootAsUartPortList(new flatbuffers.ByteBuffer(data)).unpack().ports;
}
