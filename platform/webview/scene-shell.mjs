// Browser shell: local image loading, raw events and worker recovery. C++ owns the machine,
// physics, controls and drawing. Every image gets a new worker and module instance.
import {DebugLog,captureGlobalErrors} from './debug-log.mjs';
import {mountDebugPanel} from './debug-panel.mjs';
const diagnostics=new DebugLog({source:'scene-page'}),debugPanel=mountDebugPanel(diagnostics);
captureGlobalErrors(diagnostics);
document.querySelector('#debug-open').addEventListener('click',()=>debugPanel.open());
const status=document.querySelector('#status'),report=document.querySelector('#report');
let canvas=document.querySelector('canvas');
const native=document.body.dataset.backend==='native';
const mode=new URLSearchParams(location.search).get('mode')??'probe';
const variant=mode==='probe'?'probe':'student',bench=mode==='bench';
const validMode=['probe','student','bench'].includes(mode);
document.querySelector('#mode').value=validMode?mode:'probe';
let worker=null,id=0,pending=null,chain=Promise.resolve(),dead=false,active=false,last=0,raf=0,epoch=0,upload=null,lastReport=null,sessionGeneration=0;
const heldPointers=new Set(),heldKeys=new Set();
function fatal(message){
 if(dead)return;
 diagnostics.emit('error','host.failure',{error:message,pending_request:pending?{id:pending.id,type:pending.type,elapsed_ms:performance.now()-pending.started}:null,
  classification:'host failure; not an MCU reset'});
 dead=true;active=false;++epoch;cancelAnimationFrame(raf);worker?.terminate();
 if(pending){clearTimeout(pending.timer);pending.reject(new Error(message));pending=null;}
 status.textContent=message+' — use Restart session. This is a host failure, not an MCU watchdog reset.';status.className='fault';
}
function request(type,body={},transfer=[]){
 const generation=sessionGeneration,queued=performance.now();
 const execute=()=>new Promise((resolve,reject)=>{
  if(generation!==sessionGeneration){reject(new Error("Session replaced"));return;}
  if(dead){reject(new Error('Worker is unavailable'));return;}
  const requestId=++id,started=performance.now(),timer=setTimeout(()=>{if(generation===sessionGeneration)fatal(native?'NATIVE_HOST_TIMEOUT':'WASM_HOST_TIMEOUT');},type==='init'?30000:10000);
  pending={id:requestId,type,started,resolve,reject,timer};
  diagnostics.emit(type==='frame'?'trace':'debug','request.sent',{request_id:requestId,type,queue_ms:started-queued,
   input:type==='event'?body:null});
  try{worker.postMessage({id:requestId,type,...body},transfer);}
  catch(error){diagnostics.emit('error','worker.send_failed',{request_id:requestId,type,error});fatal(error.message);}
 });
 const operation=chain.then(execute);chain=operation.catch(()=>{});return operation;
}
function send(type,body={}){const generation=sessionGeneration;return request(type,body).catch(error=>{if(!dead&&generation===sessionGeneration)fatal(error.message);});}
function release(){
 if(active||heldKeys.size||heldPointers.size)diagnostics.emit('debug','input.release_all',{keys:[...heldKeys],pointers:[...heldPointers],hidden:document.hidden});
 heldPointers.clear();heldKeys.clear();active=false;last=0;++epoch;cancelAnimationFrame(raf);
 if(worker&&!dead)void send('event',{kind:6,identifier:0,x:0,y:0});
}
function activate(){if(active||dead||!worker)return;active=true;last=0;const generation=++epoch;raf=requestAnimationFrame(now=>pump(now,generation));}
async function pump(now,generation){
 if(!active||dead||generation!==epoch)return;
 const ms=last?Math.max(0,Math.min(1000,now-last)):0;last=now;await send('frame',{ms});
 if(active&&!dead&&generation===epoch)raf=requestAnimationFrame(now=>pump(now,generation));
}
function raw(kind,identifier,x=0,y=0){void send('event',{kind,identifier,x,y});}
function point(event){const r=canvas.getBoundingClientRect();return{x:(event.clientX-r.left)*canvas.width/r.width,y:(event.clientY-r.top)*canvas.height/r.height};}
function keyCode(event){if(event.code==='Space')return 32;if(/^Key[A-Z]$/.test(event.code))return event.code.charCodeAt(3);if(/^Digit[0-9]$/.test(event.code))return event.code.charCodeAt(5);return null;}
function bindCanvas(){
 canvas.addEventListener('pointerdown',event=>{if(dead||event.button!==0)return;event.preventDefault();canvas.focus();canvas.setPointerCapture(event.pointerId);heldPointers.add(event.pointerId);const p=point(event);raw(0,event.pointerId,p.x,p.y);});
 function end(event,kind=1){if(!heldPointers.delete(event.pointerId))return;const p=point(event);raw(kind,event.pointerId,p.x,p.y);}
 canvas.addEventListener('pointerup',event=>end(event));canvas.addEventListener('pointercancel',event=>end(event,3));canvas.addEventListener('lostpointercapture',event=>end(event,3));
 canvas.addEventListener('keydown',event=>{const c=keyCode(event);if(c===null||event.repeat||heldKeys.has(c))return;event.preventDefault();heldKeys.add(c);raw(4,c);});
 canvas.addEventListener('blur',release);canvas.addEventListener('focus',activate);
}
window.addEventListener('keyup',event=>{const c=keyCode(event);if(c!==null&&heldKeys.delete(c)){event.preventDefault();raw(5,c);}});
window.addEventListener('blur',release);document.addEventListener('visibilitychange',()=>{if(document.hidden)release();else activate();});
function download(data,name){const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)],{type:'application/json'})),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
async function start(){
 const generation=++sessionGeneration;
 active=false;++epoch;cancelAnimationFrame(raf);worker?.terminate();
 if(pending){clearTimeout(pending.timer);pending.reject(new Error('Session replaced'));pending=null;}
 heldPointers.clear();heldKeys.clear();chain=Promise.resolve();dead=false;id=0;last=0;lastReport=null;report.textContent='';document.querySelector('#save-report').disabled=true;document.querySelector('#result-details').hidden=true;status.className='';status.textContent='Loading compiled application…';
 diagnostics.newSession();diagnostics.setContext({backend:native?'native':'wasm',mode,generation});
 diagnostics.emit('info','session.start',{variant,bench,upload:upload?{name:upload.name,bytes:upload.bytes.byteLength}:null});
 const next=canvas.cloneNode(false);canvas.replaceWith(next);canvas=next;bindCanvas();
 try{
  if(!validMode)throw new Error('Invalid mode');
  if(!('WebAssembly'in window)||!canvas.transferControlToOffscreen)throw new Error('WebAssembly and OffscreenCanvas are required');
  const offscreen=canvas.transferControlToOffscreen();
  worker=new Worker(new URL(native?'./native-scene-worker.mjs':'./scene-worker.mjs',import.meta.url),{type:'module'});
  worker.onmessage=({data})=>{
   if(generation!==sessionGeneration)return;
   if(data?.type==='debug-log'){if(!diagnostics.ingest(data.record))diagnostics.emit('warn','worker.invalid_log');return;}
   if(!pending||data?.id!==pending.id){fatal('Invalid worker sequence');return;}
   const p=pending;pending=null;clearTimeout(p.timer);
   diagnostics.emit(p.type==='frame'?'trace':'debug','request.completed',{request_id:p.id,type:p.type,duration_ms:performance.now()-p.started,fatal:Boolean(data.fatal)});
   if(data.fatal){diagnostics.emit('error','worker.failure',{request_id:p.id,type:p.type,error:data.details??data.error});p.reject(new Error(data.error));fatal(data.error);return;}
   if(data.result?.machine?.state)diagnostics.observe(data.result.machine.state,data.result.view);
   p.resolve(data.result);
  };
  worker.onerror=event=>{if(generation===sessionGeneration){diagnostics.emit('error','worker.error',{message:event.message,file:event.filename,line:event.lineno,column:event.colno});fatal('Worker error: '+event.message);}};
  worker.onmessageerror=()=>{if(generation===sessionGeneration)fatal('Unreadable worker message');};
  const result=await request('init',{variant,bench,debug:{session:diagnostics.session,level:diagnostics.level},memory:{policy:document.querySelector('#memory-policy').value,tier:document.querySelector('#memory-tier').value},canvas:offscreen,token:document.body.dataset.token,upload,device:upload?document.querySelector('#device').value:null},[offscreen]);
  if(generation!==sessionGeneration)return false;
  const image=result.image??{backend:'native',device:result.machine?.device,firmware:result.machine?.firmware};
  status.textContent=native?'NATIVE C++ / LOCAL PROCESS':`${image.device} / ${image.label} / SHA-256 ${image.wasm_sha256.slice(0,12)} / C++ Wasm machine + physics + drawing`;
  report.textContent=(image.memory?.warnings??[]).map(w=>'WARNING: '+w).join('\n');
  diagnostics.setContext({image});diagnostics.emit('info','session.ready',{device:image.device,firmware:image.firmware});
  for(const warning of image.memory?.warnings??[])diagnostics.emit('warn','image.memory_warning',{message:warning});
  canvas.focus();activate();return true;
 }catch(error){if(generation===sessionGeneration){diagnostics.emit('error','session.start_failed',{error});fatal(error.message);}return false;}
}
document.querySelector('#restart').addEventListener('click',()=>void start());
document.addEventListener('debug-level-change',event=>{if(worker&&!dead)void send('debug',{level:event.detail});});
document.querySelector('#mode').addEventListener('change',event=>{release();location.search='?mode='+encodeURIComponent(event.target.value);});
document.querySelector('#journal').addEventListener('click',async()=>{if(dead)return;release();const data=await send('journal');if(data)download(data,'mcu-scene-journal.json');});
document.querySelector('#load').addEventListener('click',async()=>{
 release();try{
  if(native)throw new Error('Local Wasm images need the Wasm page');
  const files=[...document.querySelector('#image-files').files],wasm=files.find(f=>f.name.endsWith('.wasm')),glue=files.find(f=>f.name.endsWith('.mjs'));
  if(files.length!==2||!wasm||!glue||wasm.name.slice(0,-5)!==glue.name.slice(0,-4))throw new Error('Choose the matching .wasm and .mjs pair from build/.../web');
  if(wasm.size>32*1024*1024||glue.size>2*1024*1024)throw new Error('Image exceeds the 32 MiB Wasm / 2 MiB loader limit');
  upload={bytes:await wasm.arrayBuffer(),glue:await glue.text(),name:wasm.name};await start();
 }catch(error){diagnostics.emit('error','image.load_rejected',{error});status.textContent=error.message;status.className='fault';}
});
document.querySelector('#test').addEventListener('click',async()=>{
 if(native)return;release();
 // Always test a fresh instance of the currently selected image, so results have a defined start.
 if(!await start())return;release();const result=await send('test');if(!result)return;
 lastReport=result;document.querySelector('#save-report').disabled=false;document.querySelector('#result-json').textContent=JSON.stringify(result,null,2);document.querySelector('#result-details').hidden=false;report.textContent=(result.identity?.memory?.warnings??[]).map(w=>'WARNING: '+w).join('\n')+'\n'+`${result.ok?'PASS':'FAIL'} · ${result.device} · ${result.firmware}\n`+result.checks.map(c=>`${c.pass?'PASS':'FAIL'}  ${c.name}`).join('\n')+'\nExecution smoke check; customer acceptance is a separate test suite.';
 diagnostics.observe(result.final);diagnostics.emit(result.ok?'info':'error','image.smoke_test',{ok:result.ok,device:result.device,checks:result.checks});
 await send('frame',{ms:0});
});
document.querySelector('#save-report').addEventListener('click',()=>{if(lastReport)download(lastReport,'mcu-execution-smoke.json');});
void start();
