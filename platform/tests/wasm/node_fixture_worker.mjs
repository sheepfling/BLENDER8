// Exercises REAL worker threads with an explicitly FAKE module; never used by the workbench.
import {parentPort} from 'node:worker_threads';
import {installWorkerHost} from '../../ui/worker-host.mjs';
import {fixtureModule} from './fixture-module.mjs';
installWorkerHost({
  addEventListener(type, handler) {if (type === 'message') parentPort.on('message', data => handler({data}));},
  postMessage(data) {parentPort.postMessage(data);},
}, async () => ({module:fixtureModule(),identity:{backend:'JS_TRANSPORT_TEST_DOUBLE'}}));
