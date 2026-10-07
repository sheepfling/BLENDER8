// Compiled-Wasm test worker. Fails if the real .mjs/.wasm products are missing.
import {parentPort,workerData} from 'node:worker_threads';
import {readFile} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';
import {resolve} from 'node:path';
import {createHash} from 'node:crypto';
import {installWorkerHost} from '../../ui/worker-host.mjs';
installWorkerHost({addEventListener(type,fn){if(type==='message')parentPort.on('message',data=>fn({data}));},postMessage(data){parentPort.postMessage(data);}},async variant=>{
  const stem=workerData.hang?'b8_hang':({student:'b8_student',probe:'b8_probe',starter:'b8_starter'})[variant];
  const url=pathToFileURL(resolve(workerData.directory,stem+'.mjs'));
  const wasmBinary=await readFile(new URL(stem+'.wasm',url));
  const {default:createModule}=await import(url.href);
  const module=await createModule({wasmBinary,print:message=>console.error(message),printErr:message=>console.error(message)});
  return {module,identity:{backend:'wasm',variant,wasm_sha256:createHash('sha256').update(wasmBinary).digest('hex')}};
});
