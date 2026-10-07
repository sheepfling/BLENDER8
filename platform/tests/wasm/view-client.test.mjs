// Transport contract tests with an explicit test double, NOT C++/Wasm execution.
import test from 'node:test';import assert from 'node:assert/strict';
import {ViewClient} from '../../webview/view-client.mjs';
function fixture(){const calls=[];const m={HEAPU8:new Uint8Array(8*1024*1024),ccall(name,type,types,args){calls.push({name,args});
 if(name==='b8_view_abi')return 1;if(name==='b8_view_width')return 320;if(name==='b8_view_height')return 240;if(name==='b8_view_pixels')return 8;
 if(name==='b8_view_status')return '{"ok":true}';if(name==='b8_view_journal')return '{"commands":[]}';return 0;}};return {m,calls};}
test('forwards raw pointer events without interpreting a speed',()=>{const{m,calls}=fixture();const v=new ViewClient(m);v.event(0,3,220,330);assert.deepEqual(calls.at(-1),{name:'b8_view_event',args:[0,3,220,330]});});
test('obtains frame pixels from module memory, not a drawing replica',()=>{const{m}=fixture();m.HEAPU8[8]=197;const v=new ViewClient(m);assert.equal(v.pixels().rgba[0],197);});
test('reacquires memory view after Wasm memory growth',()=>{const{m}=fixture();const v=new ViewClient(m);const old=v.pixels().rgba.buffer;m.HEAPU8=new Uint8Array(12*1024*1024);m.HEAPU8[8]=99;assert.notEqual(v.pixels().rgba.buffer,old);assert.equal(v.pixels().rgba[0],99);});
test('rejects malformed event and elapsed fields before invoking C++',()=>{const{m,calls}=fixture();const v=new ViewClient(m);let n=calls.length;for(const ms of [-1,NaN,Infinity,1001])assert.throws(()=>v.frame(ms));assert.throws(()=>v.event(9,0));assert.throws(()=>v.event(0,0,NaN,0));assert.equal(calls.length,n);});
test('framebuffer ranges are checked',()=>{const{m}=fixture();m.HEAPU8=new Uint8Array(10);const v=new ViewClient(m);assert.throws(()=>v.pixels(),/framebuffer/);});
test('C++ operation failure is not reported as a successful frame',()=>{const{m}=fixture();const v=new ViewClient(m);const original=m.ccall;m.ccall=(...a)=>a[0]==='b8_view_frame'?1:original(...a);assert.throws(()=>v.frame(10),/failed/);});
test('rejects incompatible scene ABI',()=>{const{m}=fixture();m.ccall=()=>99;assert.throws(()=>new ViewClient(m),/ABI/);});
test('status and journal use C++ serialization',()=>{const{m}=fixture();const v=new ViewClient(m);assert.equal(v.status().ok,true);assert.deepEqual(v.journal().commands,[]);});
