import {installWorkerHost} from './worker-host.mjs';

const MODULES = Object.freeze({student: 'b8_student', probe: 'b8_probe', starter: 'b8_starter'});
installWorkerHost(globalThis, async variant => {
  const stem = MODULES[variant];
  if (!stem) throw new Error('unknown firmware variant');
  const wasmURL = new URL(`./${stem}.wasm`, import.meta.url);
  const response = await fetch(wasmURL, {cache: 'no-store'});
  if (!response.ok) throw new Error(`Missing ${stem}.wasm (HTTP ${response.status}). Run wasm-build first.`);
  const bytes = new Uint8Array(await response.arrayBuffer());
  if (bytes.length < 8 || bytes[0] !== 0 || bytes[1] !== 97 || bytes[2] !== 115 || bytes[3] !== 109)
    throw new Error('response is not a WebAssembly binary');
  const sha256 = globalThis.crypto?.subtle
    ? Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)), b => b.toString(16).padStart(2, '0')).join('')
    : null;
  const {default: createModule} = await import(new URL(`./${stem}.mjs`, import.meta.url).href);
  const module = await createModule({
    wasmBinary: bytes,
    print: message => console.info('[B8 stdout]', message),
    printErr: message => console.error('[B8 stderr]', message),
  });
  return {module, identity: {backend: 'wasm', variant, wasm_sha256: sha256,
    fingerprint_available: sha256 !== null, bytes: bytes.length, bridge_abi: 1}};
});
