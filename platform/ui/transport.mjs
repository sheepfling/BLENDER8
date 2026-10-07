import {validateCommand, decodeReply} from './core-client.mjs';
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
               workerURL = new URL('./b8-worker.mjs', import.meta.url)} = {}) {
    if (!['student', 'probe', 'starter'].includes(variant) || typeof bench !== 'boolean')
      throw new TypeError('invalid firmware mode');
    if (!Number.isFinite(timeoutMs) || timeoutMs <= 0 || !Number.isFinite(startupTimeoutMs) || startupTimeoutMs <= 0)
      throw new RangeError('positive worker deadlines required');
    this.timeoutMs = timeoutMs; this.pending = new Map(); this.nextId = 1;
    this.failure = null; this.closed = false; this.hello = null; this.identity = null;
    this.worker = workerFactory(workerURL, {type: 'module', name: 'b8-simulation'});
    this.worker.onmessage = ({data}) => this.receive(data);
    this.worker.onerror = event => this.fail(new Error(`WASM_WORKER_ERROR: ${event.message || 'worker failed'}`));
    this.worker.onmessageerror = () => this.fail(new Error('WASM_WORKER_PROTOCOL: unreadable message'));
    this.ready = this.request('init', {variant, bench}, startupTimeoutMs).then(({result, identity}) => {
      if (!result?.ok || result.protocol !== 1) throw new Error('invalid worker handshake');
      this.hello = result; this.identity = identity; return this;
    }).catch(error => { this.fail(error); throw error; });
    // Scheduling command() before ready is allowed, but no request races initialization.
    this.chain = this.ready.then(() => {}).catch(() => {});
  }
  request(type, payload, timeout) {
    if (this.failure) return Promise.reject(this.failure);
    if (this.closed) return Promise.reject(new Error('transport is closed'));
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => this.fail(new Error(
        'WASM_HOST_TIMEOUT: worker terminated; this is NOT a simulated watchdog reset. Restart the browser session explicitly.'
      )), timeout);
      this.pending.set(id, {resolve, reject, timer});
      try { this.worker.postMessage({id, type, ...payload}); }
      catch (error) { this.fail(error); }
    });
  }
  receive(data) {
    if (this.closed || this.failure) return;
    const pending = this.pending.get(data?.id);
    if (!pending) { this.fail(new Error('WASM_WORKER_PROTOCOL: unexpected reply sequence')); return; }
    if (data.fatal) { this.fail(new Error(data.error || 'WASM_HOST_FAILURE')); return; }
    if (!data.result || typeof data.result.ok !== 'boolean') {
      this.fail(new Error('WASM_WORKER_PROTOCOL: malformed reply')); return;
    }
    clearTimeout(pending.timer); this.pending.delete(data.id); pending.resolve(data);
  }
  fail(error) {
    if (this.closed || this.failure) return;
    this.failure = error instanceof Error ? error : new Error(String(error));
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
    this.chain = task.catch(() => {});
    return task;
  }
  dispose() {
    if (this.closed) return;
    this.fail(new Error('browser session disposed'));
    this.closed = true;
  }
}
