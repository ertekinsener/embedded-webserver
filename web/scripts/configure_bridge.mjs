// Configures the Ethernet bridge of one port through the device's API, for
// tests and scripts (the web UI does the same):
//
//   node scripts/configure_bridge.mjs --url http://192.168.1.1 \
//       --out <build>/webserver_generated/web --port 1 \
//       --mode rfc2217 --local-port 4001
//   ... --mode client --remote 192.168.1.2:5000
//   ... --mode udp --local-port 4001 [--remote 192.168.1.2:5000]
//   ... --delimiter '\n' --idle-gap 20 --max-frame 512
//   ... --off
//
// Modes: tcp (raw TCP server), rfc2217, client, udp. Prints the bridge
// status afterwards.

import { parseArgs } from 'node:util';

import { loadCodec } from './load_codec.mjs';

const { values: args } = parseArgs({
  options: {
    url: { type: 'string', default: 'http://192.168.1.1' },
    out: { type: 'string' },
    user: { type: 'string', default: 'admin' },
    password: { type: 'string', default: 'admin' },
    port: { type: 'string' },
    mode: { type: 'string', default: 'tcp' },
    'local-port': { type: 'string' },
    remote: { type: 'string', default: '' },
    'idle-gap': { type: 'string', default: '0' },
    'max-frame': { type: 'string', default: '1024' },
    delimiter: { type: 'string' },
    off: { type: 'boolean', default: false },
  },
});
if (!args.out || !args.port) {
  console.error('--out <build>/webserver_generated/web and --port <id> are required');
  process.exit(2);
}

const codec = await loadCodec(args.out);
const modes = {
  tcp: codec.BridgeMode.TcpServer,
  rfc2217: codec.BridgeMode.Rfc2217,
  client: codec.BridgeMode.TcpClient,
  udp: codec.BridgeMode.Udp,
};
if (!(args.mode in modes)) {
  console.error(`unknown mode ${args.mode} (tcp, rfc2217, client, udp)`);
  process.exit(2);
}
const portId = Number(args.port);
let remoteAddress = [0, 0, 0, 0];
let remotePort = 0;
if (args.remote) {
  const [host, port] = args.remote.split(':');
  remoteAddress = host.split('.').map(Number);
  remotePort = Number(port);
}
const escapes = { '\\n': 10, '\\r': 13, '\\t': 9, '\\0': 0 };
const delimiter =
  args.delimiter === undefined
    ? null
    : (escapes[args.delimiter] ??
      (args.delimiter.startsWith('0x') ? parseInt(args.delimiter, 16) : args.delimiter.charCodeAt(0)));

async function call(method, path, body, token) {
  const headers = {};
  if (token) headers.Authorization = `Bearer ${token}`;
  if (body) headers['Content-Type'] = codec.kFlatBufferType;
  const response = await fetch(`${args.url}${path}`, { method, headers, body });
  const data = new Uint8Array(await response.arrayBuffer());
  if (!response.ok) {
    throw new Error(`${method} ${path}: ${response.status} ${new TextDecoder().decode(data)}`);
  }
  return data;
}

const login = codec.decodeLoginResponse(
  await call('POST', '/api/auth/login', codec.encodeLoginRequest(args.user, args.password)),
);
const token = codec.text(login.token);
await call(
  'POST',
  '/api/bridge/config',
  codec.encodeBridgeSettings(
    new codec.BridgeSettingsT(
      portId,
      !args.off,
      modes[args.mode],
      Number(args['local-port'] ?? 4000 + portId),
      remoteAddress,
      remotePort,
      Number(args['idle-gap']),
      Number(args['max-frame']),
      delimiter !== null,
      delimiter ?? 10,
    ),
  ),
  token,
);
await new Promise((resolve) => setTimeout(resolve, 300));  // One bridge cycle.
const port = codec
  .decodeUartPorts(await call('GET', '/api/uart/ports', undefined, token))
  .find((p) => p.id === portId);
console.log(JSON.stringify({ bridge: port.bridge, status: port.bridgeStatus }));
await call('POST', '/api/auth/logout', undefined, token);
