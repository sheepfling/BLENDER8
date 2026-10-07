/** Headless adapter for parity tests; uses the real compiled Wasm core, not an emulation in JS. */
import {readFile} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';
import {createInterface} from 'node:readline';
import {resolve} from 'node:path';
import {CoreSession, validateCommand} from '../ui/core-client.mjs';
const args = process.argv.slice(2);
const path = args.shift();
if (!path || args.some(x => !['--bench','--legacy02'].includes(x))) {
  console.error('usage: node wasm_node.mjs MODULE.mjs [--bench] [--legacy02]'); process.exit(2);
}
try {
  const moduleURL = pathToFileURL(resolve(path));
  const bytes = await readFile(new URL(moduleURL.href.replace(/\.mjs$/, '.wasm')));
  if (bytes.subarray(0, 4).toString('hex') !== '0061736d') throw new Error('missing compiled Wasm binary');
  const {default: createModule} = await import(moduleURL.href);
  const module = await createModule({wasmBinary: bytes,
    print: message => console.error('[B8 stdout]', message), printErr: message => console.error('[B8 stderr]', message)});
  const session = new CoreSession(module, args.includes('--bench'), args.includes('--legacy02'));
  console.log(JSON.stringify(session.hello));
  try {
    for await (const line of createInterface({input: process.stdin, crlfDelay: Infinity})) {
      try { validateCommand(line); }
      catch (error) { console.log(JSON.stringify({ok: false, error: error.message})); continue; }
      // A Wasm trap/malformed reply exits the process. It is not a recoverable MCU reset.
      const response = session.command(line);
      console.log(JSON.stringify(response));
      if (response.closed || response.host_failed) break;
    }
  } finally { session.dispose(); }
} catch (error) { console.error(error); process.exitCode = 2; }
