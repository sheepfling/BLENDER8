import test from 'node:test';
import assert from 'node:assert/strict';
import {DebugLog,workerLog} from '../../ui/debug-log.mjs';
import {instantiateImage} from '../../webview/image-loader.mjs';

const quiet=options=>new DebugLog({consoleSink:null,...options});
test('bounded export counts evicted records and preserves session attribution',()=>{
 const log=quiet({capacity:10,session:'first'});
 for(let i=0;i<12;i++)log.emit('info','fixture',{i});
 log.newSession('second');log.emit('warn','restart');
 const out=log.export();assert.equal(out.dropped_records,3);assert.equal(out.records.length,10);
 assert.equal(out.records[0].sequence,4);assert.equal(out.records[0].session,'first');
 assert.equal(out.records.at(-1).session,'second');assert.equal(out.records.at(-1).sequence,13);
 log.clear();assert.equal(log.export().records.length,0);assert.equal(log.dropped,0);
});
test('default level suppresses frame traffic; explicit trace enables it',()=>{
 const log=quiet();log.emit('trace','frame');log.emit('debug','request');log.emit('info','ready');
 assert.deepEqual(log.records.map(r=>r.event),['ready']);assert.equal(log.filtered,2);
 log.setLevel('trace');log.emit('trace','frame');assert.equal(log.records.at(-1).event,'frame');
});
test('wall clock, host elapsed and logical time are independently labeled',()=>{
 let host=10;const log=quiet({wallClock:()=> '2026-10-07T00:00:00Z',monotonic:()=>host});
 log.setContext({logical_us:12345});host=12;log.emit('info','sample');
 const r=log.records[0];assert.equal(r.context.logical_us,12345);assert.equal(r.host_elapsed_ms,2);
 assert.equal(r.wall_time,'2026-10-07T00:00:00Z');
});
test('export retains error stacks and redacts capabilities and local image content',()=>{
 const log=quiet(),data={token:'native-capability',authorization:'bearer',glue:'local-loader',
  bytes:new Uint8Array([1,2]),nested:{password:'private'},error:new Error('bad ABI')};
 data.self=data;log.emit('error','load_failed',data);
 const text=JSON.stringify(log.export());assert(!text.includes('native-capability'));assert(!text.includes('local-loader'));
 assert(!text.includes('bearer'));assert(!text.includes('private'));assert(text.includes('bad ABI'));assert(text.includes('debug-log.test.mjs'));
 assert.deepEqual(log.records[0].data.bytes,{type:'Uint8Array',bytes:2});assert.equal(data.token,'native-capability');
});
test('unchanged observations and repeated scene errors do not flood the log',()=>{
 const log=quiet(),state={time_us:0,ready:true,drive_enabled:false,mcu:{reset_serial:1,reset_causes:0,pwm_shadow:0}};
 log.observe(state);for(let i=0;i<100;i++){state.time_us=i;state.mcu.adc_result=i;log.observe(state);}
 assert.equal(log.records.length,1);
 state.mcu.reset_serial=2;log.observe(state);assert.equal(log.records.at(-1).event,'machine.reset_observed');
 assert.equal(log.records.at(-1).data.changes.reset_serial.to,2);
 log.observe(state,{error:'invalid operation'});log.observe(state,{error:'invalid operation'});
 assert.equal(log.records.filter(r=>r.event==='scene.operation_rejected').length,1);
});
test('worker side-channel preserves remote sequence and rejects malformed logs',()=>{
 const messages=[],worker=workerLog({postMessage:data=>messages.push(data)},'test-worker');
 worker.newSession('worker-session');worker.emit('warn','fixture.warning',{logical_us:100});
 const page=quiet();assert(page.ingest(messages[0].record));assert.equal(messages[0].type,'debug-log');
 assert.equal(page.records[0].remote_sequence,1);assert.equal(page.records[0].source,'test-worker');
 assert.equal(page.ingest({id:1,result:{ok:true}}),false);
 assert.equal(page.ingest({...messages[0].record,level:'toString'}),false);
 assert.throws(()=>page.setLevel('__proto__'),/Unknown/);
});
test('reset logs retain cleared causes and all resets between frames',()=>{
 const log=quiet(),state={time_us:0,mcu:{reset_serial:1,reset_causes:0,reset_details:0}};
 log.observe(state);
 state.time_us=400000;state.mcu.reset_serial=3;
 state.mcu.reset_history=[{serial:2,time_us:200000,causes:4,details:32,cause_names:'WDT'},
  {serial:3,time_us:300000,causes:8,details:16,cause_names:'DMT'}];
 log.observe(state);const data=log.records.at(-1).data;
 assert.equal(data.reset_events.length,2);assert.equal(data.reset_events[0].cause_names,'WDT');
 assert.equal(data.reset_events[1].causes,8);assert.equal(data.unavailable_events,0);
 assert.equal(state.mcu.reset_causes,0,'logging must not change firmware latches');
 state.mcu.reset_serial=23;state.mcu.reset_history=[];log.observe(state);
 assert.equal(log.records.at(-1).data.unavailable_events,20,'lost history must be explicit');
});
test('broken console, view or forwarding sinks cannot throw into execution',()=>{
 const log=new DebugLog({consoleSink:{error(){throw new Error('console gone');}},forward(){throw new Error('worker gone');}});
 log.subscribe(()=>{throw new Error('panel gone');});
 assert.doesNotThrow(()=>log.emit('error','failure'));assert.doesNotThrow(()=>log.clear());
});
test('loader stdout, stderr and abort messages are correlated without requiring new loader hooks',async()=>{
 const log=quiet();log.setContext({request_id:7});
 // Valid empty Wasm + fake ABI loader: tests callbacks only, never MCU behavior.
 const bytes=new Uint8Array([0,97,115,109,1,0,0,0]);
 await instantiateImage(bytes,async options=>{assert.equal(Object.hasOwn(options,'onAbort'),false);
  options.print('hello');options.printErr('failure');options.printErr('Aborted(fixture abort)');
  return {ccall:()=>1};},'TEST_LOADER',{},log);
 assert.deepEqual(log.records.filter(r=>r.event.startsWith('wasm.')).map(r=>r.event),['wasm.stdout','wasm.stderr','wasm.abort']);
 assert(log.records.every(r=>r.context.request_id===7));
});
