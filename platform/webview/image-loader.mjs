// A local image is the compiler's .wasm plus its matching Emscripten .mjs loader.
// Both are executed in a disposable worker. No bytes are uploaded to a server.
export const MAX_WASM_BYTES=32*1024*1024,MAX_GLUE_BYTES=2*1024*1024;
export function validateImage(bytes){
 if(!(bytes instanceof Uint8Array)||bytes.length<8||bytes.length>MAX_WASM_BYTES||
    ![0,97,115,109,1,0,0,0].every((v,i)=>bytes[i]===v)||!WebAssembly.validate(bytes))
   throw new Error('Invalid WebAssembly image (maximum 32 MiB)');
}
export function checkMemory(compiled,policy='warn',tier='standard',profiles){
 if(!['warn','strict','off'].includes(policy))throw new Error('Unknown memory check policy');
 const sections=WebAssembly.Module.customSections(compiled,'b8.firmware-memory');
 const warnings=[];let report=null;
 if(sections.length!==1)warnings.push('No unique firmware memory report; usage is unknown');
 else{
  try{
   report=JSON.parse(new TextDecoder().decode(sections[0]));
   const limits=profiles?.devices?.[report.device]?.[tier]??report.limits;
   if(report.schema!==1||!limits||!report.usage)throw new Error('Invalid report');
   for(const key of ['ram','program']){
    if(!Number.isSafeInteger(report.usage[key])||report.usage[key]<0)throw new Error('Invalid usage');
    if(limits[key]!==null&&report.usage[key]>limits[key])warnings.push(`${key}: ${report.usage[key]} exceeds ${limits[key]} bytes`);
   }
   warnings.push('Static section accounting only: runtime stack, heap and linked helpers are unmeasured');
  }catch{report=null;warnings.push('Invalid firmware memory report');}
 }
 // Strict rejects missing reports and measured overages, not the declared accounting boundary.
 if(policy==='strict'&&(!report||warnings.some(w=>w.includes('exceeds'))))throw new Error(warnings.join('; '));
 return {policy,tier,report,warnings:policy==='off'?[]:warnings,scope:'firmware object sections; self-declared, not authenticated'};
}
export async function instantiateImage(bytes,create,label='compiled image',options={}){
 validateImage(bytes);
 const memory=checkMemory(await WebAssembly.compile(bytes),options.policy,options.tier,options.profiles);
 const sha=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),b=>b.toString(16).padStart(2,'0')).join('');
 const module=await create({wasmBinary:bytes,locateFile:name=>new URL(name,import.meta.url).href,
   print:message=>console.info('[MCU]',message),printErr:message=>console.error('[MCU]',message)});
 if(module.ccall('b8_wasm_abi','number',[],[])!==1||module.ccall('b8_view_abi','number',[],[])!==1)
   throw new Error('This image does not implement the machine and scene ABI 1');
 return {module,identity:{label,wasm_sha256:sha,bytes:bytes.length,bridge_abi:1,scene_abi:1,backend:'wasm',memory}};
}
export async function loadImage(variant,upload,options={}){
 let bytes,create,label;
 if(upload){
   if(typeof upload.glue!=='string'||new TextEncoder().encode(upload.glue).length>MAX_GLUE_BYTES||!(upload.bytes instanceof ArrayBuffer))
     throw new Error('Choose a matching .wasm and .mjs pair');
   bytes=new Uint8Array(upload.bytes);validateImage(bytes);
   const url=URL.createObjectURL(new Blob([upload.glue],{type:'text/javascript'}));
   try{create=(await import(url)).default;}finally{URL.revokeObjectURL(url);}
   label=upload.name;
 }else{
   const stem=`b8_${variant}`;
   const response=await fetch(new URL(`./${stem}.wasm`,import.meta.url));
   if(!response.ok)throw new Error(`Missing ${stem}.wasm`);
   bytes=new Uint8Array(await response.arrayBuffer());
   create=(await import(new URL(`./${stem}.mjs`,import.meta.url))).default;label=stem+'.wasm';
 }
 options.profiles=await (await fetch(new URL('./memory-profiles.json',import.meta.url))).json();
 return instantiateImage(bytes,create,label,options);
}
// Fresh, non-invasive execution smoke check. It observes the chosen firmware; it does not
// impose a solution or interpret product acceptance. Host wall time is not MCU instruction time.
export function smokeCheck(module,identity,hello){
 const command=text=>JSON.parse(module.ccall('b8_wasm_command','string',['string','number'],[text,new TextEncoder().encode(text).length]));
 const initial=command('snapshot');
 const advanced=command('run 100000');
 const snapshot=command('snapshot');
 const checks=[
  {name:'Declared MCU/interface',pass:['B8','B16'].includes(hello.device)&&hello.interface===(hello.device==='B16'?4:3)},
  {name:'100 ms of logical machine execution',pass:advanced.ok&&advanced.state.time_us>=initial.state.time_us+100000},
  {name:'MCU ready after startup',pass:snapshot.ok&&snapshot.state.ready===true},
 ];
 return {kind:'execution-smoke',device:hello.device,firmware:hello.firmware,identity,checks,ok:checks.every(c=>c.pass),initial:initial.state,final:snapshot.state};
}
