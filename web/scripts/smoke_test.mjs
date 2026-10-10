// End-to-end check of the UI's API code (src/api/codec.ts, the generated
// FlatBuffers TypeScript and runtime) against the C++ server: starts
// webserver_host, then logs in, reads and changes UART settings over HTTP.
// Run by ctest in the host build (webserver/host/CMakeLists.txt):
//
//   node scripts/smoke_test.mjs --server <webserver_host> --out <UI build dir>
//
// or against any running server (a board, Zephyr's native_sim), with the
// checks that do not depend on the simulated ports of webserver_host:
//
//   node scripts/smoke_test.mjs --url http://192.168.1.1 --out <UI build dir>

import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';

import * as esbuild from 'esbuild';

const webDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
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

// The codec as an ES module for Node, resolved like in the UI build.
async function loadCodec() {
  const result = await esbuild.build({
    entryPoints: [path.join(webDir, 'src/api/codec.ts')],
    bundle: true,
    format: 'esm',
    platform: 'node',
    write: false,
    alias: {
      '@api': path.join(args.out, 'ts/webserver/api'),
      flatbuffers: path.join(webDir, '../../external/flatbuffers/ts/flatbuffers.ts'),
    },
  });
  const source = Buffer.from(result.outputFiles[0].contents).toString('base64');
  return import(`data:text/javascript;base64,${source}`);
}

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

const codec = await loadCodec();
const server = simulated
  ? spawn(args.server, [String(port)], { stdio: ['ignore', 'ignore', 'inherit'] })
  : null;
let token = '';

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
  await waitForServer();

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
  const login = await call(
    'POST', '/api/auth/login', codec.encodeLoginRequest(args.user, args.password),
  );
  assert.equal(login.status, 200);
  token = codec.text(codec.decodeLoginResponse(login.data).token);
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

    // SIM2 cannot change the FIFO setting.
    settings.fifoEnabled = true;
    assert.equal(
      (await call('POST', '/api/uart/config', codec.encodeUartSettings(settings))).status,
      422,
    );
  }

  assert.equal((await call('POST', '/api/auth/logout')).status, 204);
  assert.equal((await call('GET', '/api/uart/ports')).status, 401);
  console.log('UI smoke test passed');
} finally {
  server?.kill();
}
