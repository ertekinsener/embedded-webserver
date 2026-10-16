// End-to-end check of the UI's API code (src/api/codec.ts, the generated
// FlatBuffers TypeScript and runtime) against the C++ server: starts
// webserver_host, then logs in, reads and changes UART settings over HTTP,
// changes the password and restarts the server to check what it saved.
// Run by ctest in the host build (host/CMakeLists.txt):
//
//   node scripts/smoke_test.mjs --server <webserver_host> --out <UI build dir>
//
// or against any running server (a board, Zephyr's native_sim), with the
// checks that do not depend on the simulated ports of webserver_host:
//
//   node scripts/smoke_test.mjs --url http://192.168.1.1 --out <UI build dir>

import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { parseArgs } from 'node:util';

import { loadCodec } from './load_codec.mjs';

const { values: args } = parseArgs({
  options: {
    server: { type: 'string' },
    url: { type: 'string' },
    out: { type: 'string' },
    user: { type: 'string', default: 'admin' },
    password: { type: 'string', default: 'admin' },
  },
});
const port = 20000 + Math.floor(Math.random() * 20000);
const base = args.url ?? `http://127.0.0.1:${port}`;
const simulated = args.url === undefined;  // webserver_host's SIM1..SIM3

async function waitForServer() {
  for (let attempt = 0; attempt < 50; ++attempt) {
    try {
      await fetch(`${base}/favicon.svg`);
      return;
    } catch {
      await new Promise((resolve) => setTimeout(resolve, 100));
    }
  }
  throw new Error('webserver_host did not start');
}

const codec = await loadCodec(args.out);
const settingsFile = path.join(os.tmpdir(), `webserver_smoke_${process.pid}.bin`);
let server = null;
let token = '';

async function startServer() {
  server = spawn(args.server, [String(port), settingsFile], {
    stdio: ['ignore', 'ignore', 'inherit'],
  });
  await waitForServer();
}

// Like a reboot: what the server saved in settingsFile comes back.
async function restartServer() {
  const exited = new Promise((resolve) => server.once('exit', resolve));
  server.kill();
  await exited;
  token = '';
  await startServer();
}

async function login(user, password) {
  const response = await call('POST', '/api/auth/login', codec.encodeLoginRequest(user, password));
  if (response.status === 200) {
    token = codec.text(codec.decodeLoginResponse(response.data).token);
  }
  return response.status;
}

// The decoded body of a 200 answer.
async function get(url, decode) {
  const response = await call('GET', url);
  assert.equal(response.status, 200, `GET ${url}`);
  return decode(response.data);
}

async function call(method, url, body) {
  const headers = {};
  if (token) headers.Authorization = `Bearer ${token}`;
  if (body) headers['Content-Type'] = codec.kFlatBufferType;
  const response = await fetch(`${base}${url}`, { method, headers, body });
  return { status: response.status, data: new Uint8Array(await response.arrayBuffer()) };
}

try {
  if (simulated) {
    await startServer();
  } else {
    await waitForServer();
  }

  const index = await fetch(`${base}/`);
  assert.equal(index.status, 200);
  assert.equal(index.headers.get('content-type'), 'text/html');
  assert.equal(index.headers.get('content-encoding'), 'gzip');
  assert.match(await index.text(), /<div id="app"/);  // fetch un-gzips

  assert.equal((await call('GET', '/api/uart/ports')).status, 401);
  assert.equal(
    (await call('POST', '/api/auth/login', codec.encodeLoginRequest('admin', 'wrong'))).status,
    401,
  );
  assert.equal(await login(args.user, args.password), 200);
  assert.equal(token.length, 32);

  const info = await get('/api/system/info', codec.decodeSystemInfo);
  console.log(`device: ${codec.text(info.device)}, platform: ${codec.text(info.platform)}`);
  let ports = await get('/api/uart/ports', codec.decodeUartPorts);
  console.log(`ports: ${ports.map((p) => codec.text(p.name)).join(', ')}`);
  assert.ok(ports.length > 0);
  // Unknown port and an invalid body.
  assert.equal(
    (await call('POST', '/api/uart/config', codec.encodeUartSettings(new codec.UartSettingsT(200))))
      .status,
    404,
  );
  assert.equal((await call('POST', '/api/uart/config', new Uint8Array([1, 2, 3]))).status, 400);

  if (simulated) {
    assert.equal(codec.text(info.platform), 'POSIX sockets');
    assert.deepEqual(info.ipv4Address, [127, 0, 0, 1]);
    assert.deepEqual(ports.map((p) => codec.text(p.name)), ['SIM1', 'SIM2', 'SIM3']);
    assert.deepEqual(ports[1].capabilities.flowControls, [
      codec.FlowControl.None,
      codec.FlowControl.RTS_CTS,
    ]);

    const settings = new codec.UartSettingsT(
      2, 57600, codec.WordLength.Bits_7, codec.StopBits.Bits_2, codec.Parity.Even,
      codec.FlowControl.RTS_CTS, codec.Oversampling.Over16, false, false,
    );
    assert.equal(
      (await call('POST', '/api/uart/config', codec.encodeUartSettings(settings))).status,
      204,
    );
    ports = await get('/api/uart/ports', codec.decodeUartPorts);
    assert.deepEqual(
      [ports[1].settings.baudRate, ports[1].settings.wordLength, ports[1].settings.parity],
      [57600, codec.WordLength.Bits_7, codec.Parity.Even],
    );

    // Bridge: SIM1 starts on TCP 4001, so SIM2 cannot take that port.
    assert.equal(ports[0].bridge.localPort, 4001);
    assert.equal(ports[2].bridge, null);
    const bridge = (port, enabled, localPort, mode = codec.BridgeMode.TcpServer, remote = [0, 0, 0, 0], remotePort = 0) =>
      call('POST', '/api/bridge/config',
           codec.encodeBridgeSettings(new codec.BridgeSettingsT(
             port, enabled, mode, localPort, remote, remotePort, 0, 1024, false, 10)));
    assert.equal((await bridge(2, true, 4001)).status, 422);
    assert.equal((await bridge(2, true, 4002)).status, 204);
    ports = await get('/api/uart/ports', codec.decodeUartPorts);
    assert.equal(ports[1].bridge.localPort, 4002);
    assert.ok(ports[1].bridgeStatus.listening);
    assert.equal((await bridge(2, true, 0, codec.BridgeMode.TcpClient)).status, 422);
    assert.equal(
      (await bridge(2, true, 0, codec.BridgeMode.TcpClient, [192, 168, 1, 2], 5000)).status, 204);
    ports = await get('/api/uart/ports', codec.decodeUartPorts);
    assert.equal(ports[1].bridge.mode, codec.BridgeMode.TcpClient);
    assert.deepEqual(ports[1].bridge.remoteAddress, [192, 168, 1, 2]);
    assert.equal((await bridge(2, false, 4002)).status, 204);

    // SIM2 cannot change the FIFO setting.
    settings.fifoEnabled = true;
    assert.equal(
      (await call('POST', '/api/uart/config', codec.encodeUartSettings(settings))).status,
      422,
    );

    // Saved settings: the password and SIM2's settings survive a restart,
    // deleting them brings the defaults back.
    assert.ok(info.settingsSaved);
    const changePassword = (current, next) =>
      call('POST', '/api/auth/password', codec.encodeChangePasswordRequest(current, next));
    assert.equal((await changePassword('wrong', 'smoke password')).status, 403);
    assert.equal((await changePassword(args.password, 'short')).status, 422);
    assert.equal((await changePassword(args.password, 'smoke password')).status, 204);
    await restartServer();
    assert.equal(await login(args.user, args.password), 401);
    assert.equal(await login(args.user, 'smoke password'), 200);
    ports = await get('/api/uart/ports', codec.decodeUartPorts);
    assert.equal(ports[1].settings.baudRate, 57600);
    assert.equal((await call('POST', '/api/settings/reset')).status, 204);
    await restartServer();
    assert.equal(await login(args.user, args.password), 200);
    ports = await get('/api/uart/ports', codec.decodeUartPorts);
    assert.equal(ports[1].settings.baudRate, 115200);
  }

  assert.equal((await call('POST', '/api/auth/logout')).status, 204);
  assert.equal((await call('GET', '/api/uart/ports')).status, 401);
  console.log('UI smoke test passed');
} finally {
  server?.kill();
  fs.rmSync(settingsFile, { force: true });
}
