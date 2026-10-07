import test from 'node:test';
import assert from 'node:assert/strict';
import {WasmTransport, NativeTransport} from '../../ui/transport.mjs';
import {nodeWorkerFactory} from './node_worker_adapter.mjs';
const factory=nodeWorkerFactory(new URL('./node_fixture_worker.mjs', import.meta.url));
async function transport(options={}) {
  const t=new WasmTransport({workerFactory:factory,...options});await t.ready;return t;
}
test('real worker initializes through the shared ABI driver (fake module)', async () => {
  const t=await transport();try {
    assert.equal(t.hello.firmware,'JS_TRANSPORT_TEST_DOUBLE');
    assert.equal((await t.command('run 17')).state.time_us,17);
  }finally{t.dispose();}
});
test('concurrent callers are serialized, preserving logical increments', async () => {
  const t=await transport();try {
    const results=await Promise.all([t.command('run 10'),t.command('run 20'),t.command('snapshot')]);
    assert.deepEqual(results.map(r=>r.state.time_us),[10,30,30]);
  }finally{t.dispose();}
});
test('commands queued before initialization wait for readiness', async () => {
  const t=new WasmTransport({workerFactory:factory});try {
    assert.equal((await t.command('run 5')).state.time_us,5);
  }finally{t.dispose();}
});
test('ordinary command rejection is recoverable and does not poison the queue', async () => {
  const t=await transport();try {
    await assert.rejects(t.command('bad'),/test parse rejection/);
    assert.equal((await t.command('run 1')).state.time_us,1);
    assert.equal(t.failure,null);
  }finally{t.dispose();}
});
test('worker timeout kills a genuinely nonreturning worker, not a WDT event', async () => {
  const t=await transport({timeoutMs:100});
  try {
    const pending=[t.command('TEST:hang'),t.command('run 1')];
    const results=await Promise.allSettled(pending);
    assert(results.every(r=>r.status==='rejected'&&r.reason.message.includes('NOT a simulated watchdog reset')));
    assert.equal(t.pending.size,0);
    await assert.rejects(t.command('snapshot'),/TIMEOUT/);
    const fresh=await transport();try {assert.equal(fresh.hello.state.time_us,0);}finally{fresh.dispose();}
  }finally{t.dispose();}
});
test('worker trap becomes a terminal host failure, never fallback', async () => {
  const t=await transport();try {
    await assert.rejects(t.command('TEST:trap'),/WASM_HOST_FAILURE/);
    await assert.rejects(t.command('snapshot'),/WASM_HOST_FAILURE/);
  }finally{t.dispose();}
});
test('unknown reply id cannot satisfy an unrelated request', async () => {
  const t=await transport();try {
    t.receive({id:987654,result:{ok:true}});
    await assert.rejects(t.command('run 1'),/unexpected reply/);
  }finally{t.dispose();}
});
test('dispose terminates without waiting for cooperative firmware', async () => {
  const t=await transport();
  const reply=t.command('TEST:hang');
  // Bind rejection first; disposal must reject both live and queued commands.
  const checked=assert.rejects(reply,/disposed/);
  await new Promise(resolve=>setTimeout(resolve,20));t.dispose();await checked;
});
test('invalid worker selection/timeout rejected before construction', () => {
  for (const options of [{variant:'oops'},{timeoutMs:0},{startupTimeoutMs:NaN},{bench:'yes'}])
    assert.throws(()=>new WasmTransport(options));
});
test('NativeTransport preserves token, command shape and ordered replies', async () => {
  const sent=[];
  const t=new NativeTransport({token:'unit-secret',hello:{firmware:'unit',state:{}},fetchImpl:async (url,options)=>{
    sent.push([url,options]);return {text:async()=>'{"ok":true,"state":{"time_us":0}}'};
  }});
  await Promise.all([t.command('snapshot'),t.command('run 1')]);
  assert.equal(sent[0][0],'/command');
  assert.equal(sent[0][1].headers['X-B8-Token'],'unit-secret');
  assert.deepEqual(sent.map(s=>JSON.parse(s[1].body).command),['snapshot','run 1']);
  t.dispose();await assert.rejects(t.command('run 1'),/closed/);
});
