// The UI's API code (src/api/codec.ts) as an ES module for Node scripts,
// resolved like in the UI build. `generatedDir` is the UI build output of
// a build directory (<build>/webserver_generated/web).

import path from 'node:path';
import { fileURLToPath } from 'node:url';

import * as esbuild from 'esbuild';

const webDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

export async function loadCodec(generatedDir) {
  const result = await esbuild.build({
    entryPoints: [path.join(webDir, 'src/api/codec.ts')],
    bundle: true,
    format: 'esm',
    platform: 'node',
    write: false,
    alias: {
      '@api': path.join(generatedDir, 'ts/webserver/api'),
      flatbuffers: path.join(webDir, '../../external/flatbuffers/ts/flatbuffers.ts'),
    },
  });
  const source = Buffer.from(result.outputFiles[0].contents).toString('base64');
  return import(`data:text/javascript;base64,${source}`);
}
