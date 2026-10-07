import {Worker} from 'node:worker_threads';
export function nodeWorkerFactory(script, workerData = {}) {
  return () => {
    const worker = new Worker(script, {workerData});
    const adapter = {onmessage:null,onerror:null,onmessageerror:null,
      postMessage(data) {worker.postMessage(data);}, terminate() {return worker.terminate();}};
    worker.on('message', data => adapter.onmessage?.({data}));
    worker.on('error', error => adapter.onerror?.({message:error.message}));
    worker.on('messageerror', () => adapter.onmessageerror?.({}));
    return adapter;
  };
}
