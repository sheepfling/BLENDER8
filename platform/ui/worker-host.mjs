import {CoreSession, validateCommand} from './core-client.mjs';

/** Worker-local dispatcher. The only injectable dependency is the compiled-module loader. */
export function installWorkerHost(scope, loadModule) {
  let session = null;
  let begun = false;
  let busy = false;
  let previousId = 0;
  scope.addEventListener('message', async ({data}) => {
    let id = data?.id;
    if (!Number.isSafeInteger(id) || id <= previousId) {
      scope.postMessage({id, fatal: true, error: 'invalid or repeated worker request sequence'});
      return;
    }
    previousId = id;
    if (busy) {
      scope.postMessage({id, fatal: true, error: 'worker requests must be serialized'});
      return;
    }
    busy = true;
    try {
      if (data.type === 'init') {
        if (begun) throw new Error('worker already initialized');
        begun = true;
        if (!['student', 'probe', 'starter'].includes(data.variant) || typeof data.bench !== 'boolean')
          throw new TypeError('invalid firmware variant or bench mode');
        const {module, identity} = await loadModule(data.variant);
        session = new CoreSession(module, data.bench);
        scope.postMessage({id, result: session.hello, identity});
      } else if (data.type === 'command') {
        if (!session) throw new Error('worker has no initialized session');
        try { validateCommand(data.command); }
        catch (error) { scope.postMessage({id, result: {ok: false, error: error.message}}); return; }
        const result = session.command(data.command);
        scope.postMessage({id, result});
      } else if (data.type === 'dispose') {
        session?.dispose(); session = null;
        scope.postMessage({id, result: {ok: true, closed: true}});
      } else throw new TypeError('unknown worker request');
    } catch (error) {
      // JS/wasm traps are host failures. Do not invent a peripheral-reset diagnostic.
      scope.postMessage({id, fatal: true, error: `WASM_HOST_FAILURE: ${error?.message ?? String(error)}`});
    } finally { busy = false; }
  });
}
