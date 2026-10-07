import {validateCommand, decodeReply} from './core-client.mjs';
import {DebugLog} from './debug-log.mjs';
export class NativeTransport {
  constructor({token, hello, endpoint = '/command', fetchImpl = globalThis.fetch.bind(globalThis)}) {
    this.token = token; this.hello = hello; this.endpoint = endpoint; this.fetch = fetchImpl;
    this.identity = {backend: 'native', firmware: hello.firmware};
    this.closed = false; this.chain = Promise.resolve();
  }
  command(command) {
    validateCommand(command);
    const task = this.chain.then(async () => {
      if (this.closed) throw new Error('transport is closed');
      const response = await this.fetch(this.endpoint, {method: 'POST',
        headers: {'Content-Type': 'application/json', 'X-B8-Token': this.token},
        body: JSON.stringify({command})});
      const result = decodeReply(await response.text());
      if (!result.ok) throw new Error(result.error ?? 'native command rejected');
      if (result.closed) this.closed = true;
      return result;
    });
    this.chain = task.catch(() => {});
    return task;
  }
  dispose() { this.closed = true; }
}

export class WasmTransport {
  constructor({variant = 'student', bench = false, timeoutMs = 10000, startupTimeoutMs = 30000,
               workerFactory = (url, options) => new Worker(url, options),
               workerURL = new URL('./b8-worker.mjs', import.meta.url),
               log = new DebugLog({source:'diagnostics-page'})} = {}) {
    if (!['student', 'probe', 'starter'].includes(variant) || typeof bench !== 'boolean')
      throw new TypeError('invalid firmware mode');
    if (!Number.isFinite(timeoutMs) || timeoutMs <= 0 || !Number.isFinite(startupTimeoutMs) || startupTimeoutMs <= 0)
      throw new RangeError('positive worker deadlines required');
    this.timeoutMs = timeoutMs; this.pending = new Map(); this.nextId = 1;
    this.log=log;this.log.setContext({backend:'wasm',variant,bench});
    this.failure = null; this.closed = false; this.hello = null; this.identity = null;
    this.worker = workerFactory(workerURL, {type: 'module', name: 'b8-simulation'});
    this.worker.onmessage = ({data}) => this.receive(data);
    this.worker.onerror = event => this.fail(new Error(`WASM_WORKER_ERROR: ${event.message || 'worker failed'}`));
    this.worker.onmessageerror = () => this.fail(new Error('WASM_WORKER_PROTOCOL: unreadable message'));
    this.ready = this.request('init', {variant, bench,debug:{session:log.session,level:log.level}}, startupTimeoutMs).then(({result, identity}) => {
      if (!result?.ok || result.protocol !== 1) throw new Error('invalid worker handshake');
      this.hello = result; this.identity = identity;this.log.setContext({device:result.device,image:identity});
      this.log.observe(result.state);this.log.emit('info','session.ready',{firmware:result.firmware,device:result.device});return this;
    }).catch(error => { this.fail(error); throw error; });
    // Scheduling command() before ready is allowed, but no request races initialization.
    this.chain = this.ready.then(() => {}).catch(() => {});
  }
  request(type, payload, timeout) {
    if (this.failure) return Promise.reject(this.failure);
    if (this.closed) return Promise.reject(new Error('transport is closed'));
    const id = this.nextId++;
    const started=performance.now();
    const level=type==='command'&&/^(run|snapshot)\b/.test(payload.command)?'trace':'debug';
    this.log.emit(level,'request.sent',{request_id:id,type,command:payload.command});
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => this.fail(new Error(
        'WASM_HOST_TIMEOUT: worker terminated; this is NOT a simulated watchdog reset. Restart the browser session explicitly.'
      )), timeout);
      this.pending.set(id, {resolve, reject, timer,started,type,level,command:payload.command});
      try { this.worker.postMessage({id, type, ...payload}); }
      catch (error) { this.fail(error); }
    });
  }
  receive(data) {
    if (this.closed || this.failure) return;
    if(data?.type==='debug-log'){if(!this.log.ingest(data.record))this.log.emit('warn','worker.invalid_log');return;}
    const pending = this.pending.get(data?.id);
    if (!pending) { this.fail(new Error('WASM_WORKER_PROTOCOL: unexpected reply sequence')); return; }
    if (data.fatal) { this.log.emit('error','worker.failure',{request_id:data.id,error:data.details??data.error});this.fail(new Error(data.error || 'WASM_HOST_FAILURE')); return; }
    if (!data.result || typeof data.result.ok !== 'boolean') {
      this.fail(new Error('WASM_WORKER_PROTOCOL: malformed reply')); return;
    }
    clearTimeout(pending.timer); this.pending.delete(data.id);
    this.log.emit(pending.level,'request.completed',{request_id:data.id,type:pending.type,duration_ms:performance.now()-pending.started,command:pending.command,ok:data.result.ok});
    if(data.result.state)this.log.observe(data.result.state);
    pending.resolve(data);
  }
  fail(error) {
    if (this.closed || this.failure) return;
    this.failure = error instanceof Error ? error : new Error(String(error));
    this.log.emit('error','host.failure',{error:this.failure,pending_requests:[...this.pending].map(([id,p])=>({id,type:p.type,command:p.command,elapsed_ms:performance.now()-p.started})),classification:'host failure; not an MCU reset'});
    this.worker.terminate();
    for (const {reject, timer} of this.pending.values()) { clearTimeout(timer); reject(this.failure); }
    this.pending.clear();
  }
  command(command) {
    validateCommand(command);
    const task = this.chain.then(async () => {
      await this.ready;
      const {result} = await this.request('command', {command}, this.timeoutMs);
      if (result.host_failed) this.fail(new Error(result.error || 'simulation host-failure inhibit'));
      if (!result.ok) throw new Error(result.error || 'command rejected');
      if (result.closed) this.dispose();
      return result;
    });
    this.chain = task.catch(error => {this.log.emit(this.failure?'error':'warn','command.rejected',{command,error});});
    return task;
  }
  setLogLevel(level){
    this.log.setLevel(level);
    const task=this.chain.then(async()=>{await this.ready;await this.request('debug',{level},this.timeoutMs);});
    this.chain=task.catch(()=>{});return task;
  }
  dispose() {
    if (this.closed) return;
    // Explicit disposal is a lifecycle event, not an observed host or MCU failure.
    this.log.emit('info','session.disposed');
    this.failure??=new Error('browser session disposed');this.worker.terminate();
    for(const {reject,timer} of this.pending.values()){clearTimeout(timer);reject(this.failure);}
    this.pending.clear();
    this.closed = true;
  }
}
