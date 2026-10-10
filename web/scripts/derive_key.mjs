// Prints the PBKDF2-HMAC-SHA-256 key (32 bytes, hex) of a password, for
// the credentials compiled into the firmware (cmake/credentials.cmake):
//
//   node derive_key.mjs <password> <salt hex> <iterations>

import { pbkdf2Sync } from 'node:crypto';

const [password, saltHex, iterations] = process.argv.slice(2);
if (password === undefined || saltHex === undefined || iterations === undefined) {
  console.error('usage: derive_key.mjs <password> <salt hex> <iterations>');
  process.exit(1);
}
const key = pbkdf2Sync(password, Buffer.from(saltHex, 'hex'), Number(iterations), 32, 'sha256');
process.stdout.write(key.toString('hex'));
