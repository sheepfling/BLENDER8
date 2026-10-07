// Load bytes read from a local compiler output, using the same initializer as the file-upload path.
import test from 'node:test';import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';import {pathToFileURL} from 'node:url';
import {instantiateImage,smokeCheck,validateImage} from '../../webview/image-loader.mjs';
import {ViewClient} from '../../webview/view-client.mjs';
const web=process.env.B8_WASM_WEB;if(!web)throw new Error('Real Wasm output required');
async function uploaded(){const bytes=new Uint8Array(await readFile(web+'/b8_student.wasm'));
 const create=(await import(pathToFileURL(web+'/b8_student.mjs'))).default;
 const {module,identity}=await instantiateImage(bytes,create,'local b8_student.wasm');
 assert.equal(module.ccall('b8_wasm_init','number',['number'],[0]),0);
 return {module,identity,hello:JSON.parse(module.ccall('b8_wasm_hello','string',[],[]))};}
test('local compiled bytes execute a fresh selected device and render real C++ pixels',async()=>{
 const {module,identity,hello}=await uploaded();const view=new ViewClient(module);
 const result=smokeCheck(module,identity,hello);assert.equal(result.ok,true);assert.equal(result.final.time_us,100000);
 assert.equal(result.device,process.env.B8_EXPECT_DEVICE??'B8');assert.equal(result.identity.wasm_sha256.length,64);
 assert.equal(view.pixels().rgba.length,1280*900*4);module.ccall('b8_wasm_dispose',null,[],[]);
});
test('loading again starts with independent time and firmware globals',async()=>{
 const a=await uploaded();smokeCheck(a.module,a.identity,a.hello);const b=await uploaded();
 assert.equal(b.hello.state.time_us,0);assert.equal(b.hello.device,a.hello.device);
 a.module.ccall('b8_wasm_dispose',null,[],[]);b.module.ccall('b8_wasm_dispose',null,[],[]);
});
test('malformed and non-Wasm input are rejected before loading code',()=>{
 assert.throws(()=>validateImage(new Uint8Array([0,97,115,109])),/Invalid WebAssembly/);
 assert.throws(()=>validateImage(new Uint8Array([0,97,115,109,1,0,0,0,255])),/Invalid WebAssembly/);
});
