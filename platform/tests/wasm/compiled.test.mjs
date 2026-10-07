// Requires actual Emscripten build products; never substitutes a JS module.
import test from 'node:test';
import assert from 'node:assert/strict';
import {existsSync} from 'node:fs';
import {WasmTransport} from '../../ui/transport.mjs';
import {nodeWorkerFactory} from './node_worker_adapter.mjs';
const directory=process.env.B8_WASM_WEB;
if(!directory||!existsSync(directory+'/b8_probe.wasm'))throw new Error('Set B8_WASM_WEB to the real build-wasm/web directory.');
async function open(variant='probe',options={}){
 const t=new WasmTransport({variant,workerFactory:nodeWorkerFactory(new URL('./node_core_worker.mjs',import.meta.url),{directory,hang:options.hang}),timeoutMs:options.timeoutMs??10000});await t.ready;return t;
}
test('real C++ probe in a Wasm worker produces scanned pixels and no drive',async()=>{
 const t=await open();try{await t.command('run 100000');await t.command('speed 4');const s=(await t.command('run 100000')).state;
 assert(s.lcd_pixels.includes('1'));assert.equal(s.drive_enabled,false);assert.equal(s.contacts,8);assert.equal(t.identity.wasm_sha256.length,64);
 }finally{t.dispose();}
});
test('separate Wasm module instances do not share globals or time',async()=>{
 const a=await open(),b=await open();try{await a.command('run 100000');await a.command('speed 7');await a.command('run 10000');
 assert.equal((await b.command('snapshot')).state.time_us,0);assert.equal((await b.command('snapshot')).state.contacts,0);
 }finally{a.dispose();b.dispose();}
});
test('nonreturning compiled C++ is worker failure, not emulated WDT recovery',async()=>{
 if(!existsSync(directory+'/b8_hang.wasm'))throw new Error('Compile with --test-fixtures to run hang coverage.');
 const t=await open('student',{hang:true,timeoutMs:500});try{
 await assert.rejects(t.command('run 100000'),/WASM_HOST_TIMEOUT/);
 assert.equal(t.hello.state.time_us,0); // no invented reset/final-state response
 }finally{t.dispose();}
});
