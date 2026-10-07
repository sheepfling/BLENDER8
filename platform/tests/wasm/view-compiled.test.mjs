// Requires real compiler output. Missing files FAIL; no module doubles here.
import test from 'node:test';import assert from 'node:assert/strict';
import {readFile,access} from 'node:fs/promises';import {pathToFileURL} from 'node:url';
import {ViewClient} from '../../webview/view-client.mjs';
const web=process.env.B8_WASM_WEB;
if(!web)throw new Error('B8_WASM_WEB must identify a compiled Emscripten build');
async function load(bench=false){await access(web+'/b8_probe.wasm');const create=(await import(pathToFileURL(web+'/b8_probe.mjs'))).default;
 const m=await create({wasmBinary:await readFile(web+'/b8_probe.wasm')});assert.equal(m.ccall('b8_wasm_init','number',['number'],[bench?1:0]),0);return {m,v:new ViewClient(m)};}
function cmd(m,s){return JSON.parse(m.ccall('b8_wasm_command','string',['string','number'],[s,Buffer.byteLength(s)]));}
test('actual Wasm C++ renderer produces a nonuniform RGBA image',async()=>{const{v}=await load();const f=v.pixels();assert.equal(f.width,1280);assert.equal(f.height,900);assert(new Set(f.rgba).size>50);for(let i=3;i<f.rgba.length;i+=4)assert.equal(f.rgba[i],255);});
test('drawing and resizing do not advance or mutate the B8',async()=>{const{m,v}=await load();cmd(m,'run 80000');const before=cmd(m,'snapshot');v.frame(0);v.resize(640,450);v.frame(16);assert.deepEqual(cmd(m,'snapshot'),before);});
test('raw click reaches real mechanics and compiled probe LCD',async()=>{const{m,v}=await load();v.event(0,2,270,660);v.event(1,2,-10,-10);cmd(m,'run 150000');v.frame(0);let s=cmd(m,'snapshot').state;assert.equal(s.contacts,4);assert(s.lcd_pixels.includes('1'));assert.equal(s.drive_enabled,false);});
test('held pulse and STOP suppression are different observations',async()=>{const{m,v}=await load();v.event(4,80);v.event(4,32);cmd(m,'run 60000');v.event(5,32);cmd(m,'run 10000');assert.equal(cmd(m,'snapshot').state.run_permit,false);v.event(5,80);v.event(4,80);cmd(m,'run 10000');assert.equal(cmd(m,'snapshot').state.run_permit,true);v.event(6);assert.equal(cmd(m,'snapshot').state.run_permit,false);});
test('independent compiled scenes do not share image or machine state',async()=>{const a=await load(),b=await load();a.v.event(4,51);cmd(a.m,'run 100000');assert.equal(cmd(b.m,'snapshot').state.time_us,0);assert.equal(cmd(b.m,'snapshot').state.contacts,0);});

test('merged thermal/truth, motor and subsystem pages render without stepping the compiled machine',async()=>{
 const {m,v}=await load();const before=cmd(m,'snapshot');const hashes=new Set();
 for(const [x,page] of [[307,2],[427,5],[512,4],[629,6],[741,7]]){
  v.event(0,1,x,118);v.event(1,1,x,118);v.frame(0);
  const status=v.status();assert.equal(status.view.page,page);hashes.add(status.view.frame_hash);
  assert.deepEqual(cmd(m,'snapshot'),before);
 }
 assert.equal(hashes.size,5);
});

test('compiled LCD capture retains exact bytes and VBLANK without observational bus accesses',async()=>{
 const {m,v}=await load(true);cmd(m,'run 80000');cmd(m,'write 0x80 1');cmd(m,'write 0x81 129');
 const before=cmd(m,'snapshot');const b=before.state.lcd_bus;
 assert.equal(b.data.at(-1).data,129);assert.equal(b.data.at(-1).accepted,true);
 assert(b.blank_edges.some(e=>e.time_us>=b.epoch_us&&(e.time_us-b.epoch_us)%20000===16000&&e.high));
 assert(b.blank_edges.some(e=>e.time_us>b.epoch_us&&(e.time_us-b.epoch_us)%20000===0&&!e.high&&!e.reset));
 v.event(4,76);v.event(5,76);v.frame(0);assert.equal(v.status().view.page,7);
 assert.deepEqual(cmd(m,'snapshot'),before);
});

test('focus release frees momentary input without pausing selected logical RUN',async()=>{
 const {m,v}=await load();v.event(4,82);v.event(5,82);v.event(4,80);v.event(7);
 assert.equal(v.status().view.running,true);assert.equal(cmd(m,'snapshot').state.run_permit,false);
 v.frame(20);assert.equal(cmd(m,'snapshot').state.time_us,20000);
});
test('compiled food fixture changes physical load and thermal properties and is journaled',async()=>{
 const {m,v}=await load();v.event(0,7,960,537);v.event(1,7,960,537);
 const s=cmd(m,'snapshot').state;assert.equal(s.thermal.preset,'frozen_fruit');assert.equal(s.thermal.food_c,-10);
 assert.equal(s.thermal.load,.8);assert.equal(s.thermal.capacity_j_per_k,600);
 assert(v.journal().commands.includes('food_preset frozen_fruit'));
 const hash=v.status().view.frame_hash;v.frame(50);assert.notEqual(v.status().view.frame_hash,hash);
 assert.equal(cmd(m,'snapshot').state.time_us,0,'animation advanced physics while paused');
});
