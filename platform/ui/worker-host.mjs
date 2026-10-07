import {CoreSession, validateCommand} from './core-client.mjs';
import {workerLog,captureGlobalErrors} from './debug-log.mjs';

/** Worker-local dispatcher. The only injectable dependency is the compiled-module loader. */
export function installWorkerHost(scope, loadModule) {
  const log=workerLog(scope,'core-worker');captureGlobalErrors(log,scope);
  let session = null;
  let begun = false;
  let busy = false;
  let previousId = 0;
  scope.addEventListener('message', async ({data}) => {
    let id = data?.id;
    if (!Number.isSafeInteger(id) || id <= previousId) {
      log.emit('error','worker.invalid_sequence',{request_id:id,previous_id:previousId});
      scope.postMessage({id, fatal: true, error: 'invalid or repeated worker request sequence'});
      return;
    }
    previousId = id;
    if (busy) {
      log.emit('error','worker.concurrent_request',{request_id:id});
      scope.postMessage({id, fatal: true, error: 'worker requests must be serialized'});
      return;
    }
    busy = true;
    log.setContext({request_id:id,request_type:data.type});
    try {
      if (data.type === 'init') {
        log.newSession(data.debug?.session);log.setLevel(data.debug?.level??'info');
        log.setContext({request_id:id,request_type:data.type,variant:data.variant,bench:data.bench});
        log.emit('info','worker.initializing');
        if (begun) throw new Error('worker already initialized');
        begun = true;
        if (!['student', 'probe', 'starter'].includes(data.variant) || typeof data.bench !== 'boolean')
          throw new TypeError('invalid firmware variant or bench mode');
        const {module, identity} = await loadModule(data.variant,log);
        session = new CoreSession(module, data.bench);
        log.setContext({device:session.hello.device,image_sha256:identity.wasm_sha256});
        log.emit('info','worker.ready',{firmware:session.hello.firmware});
        scope.postMessage({id, result: session.hello, identity});
      } else if (data.type === 'command') {
        if (!session) throw new Error('worker has no initialized session');
        try { validateCommand(data.command); }
        catch (error) { log.emit('warn','command.invalid',{error});scope.postMessage({id, result: {ok: false, error: error.message}}); return; }
        const result = session.command(data.command);
        scope.postMessage({id, result});
      } else if(data.type==='debug'){
        log.setLevel(data.level);scope.postMessage({id,result:{ok:true}});
      } else if (data.type === 'dispose') {
        session?.dispose(); session = null;
        scope.postMessage({id, result: {ok: true, closed: true}});
      } else throw new TypeError('unknown worker request');
    } catch (error) {
      // JS/wasm traps are host failures. Do not invent a peripheral-reset diagnostic.
      const record=log.emit('error','wasm.host_failure',{error});
      scope.postMessage({id, fatal: true, error: `WASM_HOST_FAILURE: ${error?.message ?? String(error)}`,details:record?.data});
    } finally { busy = false; }
  });
}
