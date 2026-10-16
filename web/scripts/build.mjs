// Builds the web UI and turns it into C++ sources for the firmware.
//
//   node scripts/build.mjs --out <dir> [--flatc <path>] [--no-typecheck]
//   node scripts/build.mjs --out <dir> --typecheck-only
//
// Steps: flatc --ts for ../schema/*.fbs, type check (tsc), bundle src/main.ts
// with esbuild (the FlatBuffers runtime comes from the flatbuffers submodule,
// so it always matches flatc), compile Tailwind, give the assets
// content-hashed names, gzip everything and write
//   <out>/dist/...           the files as served (uncompressed, for a look)
//   <out>/web_resources.h    WebResources() and WEBSERVER_WEB_RESOURCES
//   <out>/web_resources.cc   the gzipped files as byte arrays
// Outputs are only rewritten when their content changes.

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';
import zlib from 'node:zlib';

import * as esbuild from 'esbuild';

const webDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const repoDir = path.resolve(webDir, '..');

const { values: args } = parseArgs({
  options: {
    out: { type: 'string' },
    flatc: { type: 'string', default: 'flatc' },
    schemas: { type: 'string', default: path.join(webDir, '../schema') },
    'flatbuffers-ts': {
      type: 'string',
      default: path.join(repoDir, 'external/flatbuffers/ts'),
    },
    'no-typecheck': { type: 'boolean', default: false },
    'typecheck-only': { type: 'boolean', default: false },
  },
});

const outDir = path.resolve(args.out ?? path.join(webDir, 'build'));
const tsOutDir = path.join(outDir, 'ts');
const distDir = path.join(outDir, 'dist');
// Generated modules are imported one by one ("@api/uart-settings"): flatc
// writes the index webserver/api.ts per schema, so it lists only the last.
const apiDir = path.join(tsOutDir, 'webserver/api');
const flatbuffersEntry = path.join(args['flatbuffers-ts'], 'flatbuffers.ts');

// Writes only if the content differs, so unchanged outputs keep their
// timestamps and nothing downstream rebuilds.
function writeIfChanged(file, content) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  const data = Buffer.isBuffer(content) ? content : Buffer.from(content);
  if (fs.existsSync(file) && fs.readFileSync(file).equals(data)) {
    return;
  }
  fs.writeFileSync(file, data);
}

function run(command, commandArgs, options = {}) {
  execFileSync(command, commandArgs, { stdio: 'inherit', ...options });
}

function generateFlatBuffers() {
  const schemas = fs
    .readdirSync(args.schemas)
    .filter((name) => name.endsWith('.fbs'))
    .map((name) => path.join(args.schemas, name));
  fs.rmSync(tsOutDir, { recursive: true, force: true });
  run(args.flatc, ['--ts', '--gen-object-api', '-o', tsOutDir, ...schemas]);
}

function typecheck() {
  const config = {
    extends: path.join(webDir, 'tsconfig.json'),
    compilerOptions: {
      noEmit: true,
      paths: { '@api/*': [`${apiDir}/*`], flatbuffers: [flatbuffersEntry] },
    },
    include: [path.join(webDir, 'src')],
  };
  const configFile = path.join(outDir, 'tsconfig.json');
  writeIfChanged(configFile, JSON.stringify(config, null, 2));
  run(path.join(webDir, 'node_modules/.bin/tsc'), ['-p', configFile]);
}

async function bundleScript() {
  const result = await esbuild.build({
    entryPoints: [path.join(webDir, 'src/main.ts')],
    bundle: true,
    minify: true,
    format: 'esm',
    target: 'es2020',
    write: false,
    legalComments: 'eof',
    alias: { '@api': apiDir, flatbuffers: flatbuffersEntry },
    logLevel: 'warning',
  });
  return result.outputFiles[0].contents;
}

function compileStyles() {
  const cssFile = path.join(outDir, 'app.css');
  run(path.join(webDir, 'node_modules/.bin/tailwindcss'), [
    '--input', path.join(webDir, 'src/styles.css'),
    '--output', cssFile,
    '--minify',
  ], { cwd: webDir, stdio: ['ignore', 'ignore', 'inherit'] });
  return fs.readFileSync(cssFile);
}

function hashed(name, content) {
  const hash = createHash('sha256').update(content).digest('hex').slice(0, 8);
  const extension = path.extname(name);
  return `${path.basename(name, extension)}-${hash}${extension}`;
}

const kContentTypes = {
  '.html': 'text/html',
  '.js': 'text/javascript',
  '.css': 'text/css',
  '.svg': 'image/svg+xml',
};
const kImmutable = 'public, max-age=31536000, immutable';

// [{ path, content, cacheControl }] as served.
async function buildFiles() {
  const script = await bundleScript();
  const styles = compileStyles();
  const scriptPath = `/assets/${hashed('app.js', script)}`;
  const stylesPath = `/assets/${hashed('app.css', styles)}`;
  const html = fs
    .readFileSync(path.join(webDir, 'index.html'), 'utf8')
    .replaceAll('{{APP_JS}}', scriptPath)
    .replaceAll('{{APP_CSS}}', stylesPath);
  return [
    { path: '/index.html', content: Buffer.from(html), cacheControl: 'no-cache' },
    { path: scriptPath, content: Buffer.from(script), cacheControl: kImmutable },
    { path: stylesPath, content: styles, cacheControl: kImmutable },
    {
      path: '/favicon.svg',
      content: fs.readFileSync(path.join(webDir, 'favicon.svg')),
      cacheControl: 'public, max-age=86400',
    },
  ];
}

function cString(text) {
  return JSON.stringify(text);
}

function byteArray(data) {
  const lines = [];
  for (let i = 0; i < data.length; i += 16) {
    lines.push(
      '    ' +
        Array.from(data.subarray(i, i + 16), (b) => `0x${b.toString(16).padStart(2, '0')},`).join(''),
    );
  }
  return lines.join('\n');
}

function writeCpp(files) {
  // "/" serves index.html as well.
  const entries = [];
  const arrays = files.map((file, index) => {
    const gzipped = zlib.gzipSync(file.content, { level: 9 });
    const useGzip = gzipped.length < file.content.length;
    return {
      ...file,
      name: `kFile${index}`,
      data: useGzip ? gzipped : file.content,
      encoding: useGzip ? 'gzip' : '',
      contentType: kContentTypes[path.extname(file.path)] ?? 'application/octet-stream',
    };
  });
  for (const file of arrays) {
    if (file.path === '/index.html') {
      entries.push({ ...file, path: '/' });
    }
    entries.push(file);
  }

  const header = `// Generated by web/scripts/build.mjs. Do not edit.
#ifndef WEBSERVER_GENERATED_WEB_RESOURCES_
#define WEBSERVER_GENERATED_WEB_RESOURCES_

#include <cstdint>
#include <span>

#include "webserver/resource_registry.h"

namespace webserver {

namespace web_data {
${arrays.map((file) => `extern const std::uint8_t ${file.name}[${file.data.length}];  // ${file.path}`).join('\n')}
}  // namespace web_data

// The files of the web UI, for ResourceRegistry.
std::span<const WebResource> WebResources() noexcept;

}  // namespace webserver

// The same list for HTTP servers that register files up front (Zephyr):
// X(<args>..., n, path, content_type, content_encoding, cache_control, data)
#define WEBSERVER_WEB_RESOURCES(X, ...) \\
${entries
  .map(
    (file, n) =>
      `  X(__VA_ARGS__, ${n}, ${cString(file.path)}, ${cString(file.contentType)}, ` +
      `${cString(file.encoding)}, ${cString(file.cacheControl)}, ::webserver::web_data::${file.name})`,
  )
  .join(' \\\n')}

#endif  // WEBSERVER_GENERATED_WEB_RESOURCES_
`;

  const source = `// Generated by web/scripts/build.mjs. Do not edit.
#include "web_resources.h"

#include <cstdint>
#include <span>

namespace webserver {
namespace web_data {

${arrays
  .map(
    (file) =>
      `// ${file.path}, ${file.content.length} B${file.encoding ? `, gzip ${file.data.length} B` : ''}\n` +
      `alignas(4) const std::uint8_t ${file.name}[${file.data.length}] = {\n${byteArray(file.data)}\n};`,
  )
  .join('\n\n')}

}  // namespace web_data
namespace {

const WebResource kResources[] = {
${entries
  .map(
    (file) =>
      `    {${cString(file.path)}, ${cString(file.contentType)}, ${cString(file.encoding)}, ` +
      `${cString(file.cacheControl)}, web_data::${file.name}},`,
  )
  .join('\n')}
};

}  // namespace

std::span<const WebResource> WebResources() noexcept { return kResources; }

}  // namespace webserver
`;

  writeIfChanged(path.join(outDir, 'web_resources.h'), header);
  writeIfChanged(path.join(outDir, 'web_resources.cc'), source);
  return arrays;
}

generateFlatBuffers();
if (!args['no-typecheck']) {
  typecheck();
}
if (!args['typecheck-only']) {
  const files = await buildFiles();
  for (const file of files) {
    writeIfChanged(path.join(distDir, file.path), file.content);
  }
  const embedded = writeCpp(files);
  const total = embedded.reduce((sum, file) => sum + file.data.length, 0);
  console.log(
    `web UI: ${embedded.map((f) => `${f.path} ${f.data.length} B`).join(', ')}; ` +
      `${total} B embedded`,
  );
}
