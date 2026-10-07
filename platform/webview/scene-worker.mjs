import {ViewClient} from './view-client.mjs';
import {loadImage,smokeCheck} from './image-loader.mjs';
import {workerLog,captureGlobalErrors} from './debug-log.mjs';
const log=workerLog(self,'scene-worker');captureGlobalErrors(log,self);
// The worker owns both the compiled machine AND the compiled software renderer.
let identity=null,hello=null,module=null,view=null,canvas=null,ctx=null,previousId=0,busy=false,failed=false;
self.addEventListener('message',async({data})=>{
  const id=data?.id;
  if(failed)return;
  if(!Number.isSafeInteger(id)||id<=previousId||busy){failed=true;log.emit('error','worker.invalid_sequence',{request_id:id,previous_id:previousId,busy});self.postMessage({id,fatal:true,error:'Invalid/out-of-order scene request'});return;}
  previousId=id;busy=true;
  log.setContext({request_id:id,request_type:data.type});
  try {
    if(data.type==='init') {
      log.newSession(data.debug?.session);log.setLevel(data.debug?.level??'info');
      log.setContext({request_id:id,request_type:data.type,variant:data.variant,bench:data.bench});
      log.emit('info','worker.initializing');
      if(view||module)throw new Error('Worker already initialized');
      if(!['probe','student','starter'].includes(data.variant)||typeof data.bench!=='boolean')throw new Error('Invalid image selection');
      canvas=data.canvas;
      if(!canvas||typeof canvas.getContext!=='function')throw new Error('OffscreenCanvas is required');
      ctx=canvas.getContext('2d',{alpha:false});
      if(!ctx)throw new Error('OffscreenCanvas 2D unavailable');
      const loaded=await loadImage(data.variant,data.upload,data.memory,log);
      module=loaded.module;identity=loaded.identity;
      if(module.ccall('b8_wasm_init','number',['number'],[data.bench?1:0]))throw new Error('Machine initialization failed');
      hello=JSON.parse(module.ccall('b8_wasm_hello','string',[],[]));
      if(!hello.ok||!['B8','B16'].includes(hello.device))throw new Error('Image has no supported MCU identity');
      if(data.device&&data.device!==hello.device)throw new Error(`Selected ${data.device}, image is ${hello.device}`);
      log.setContext({device:hello.device,image_sha256:identity.wasm_sha256});
      log.emit('info','worker.ready',{firmware:hello.firmware,bridge_abi:identity.bridge_abi,scene_abi:identity.scene_abi});
      view=new ViewClient(module); view.resize(canvas.width,canvas.height);
    } else {
      if(!view)throw new Error('Scene not initialized');
      if(data.type==='frame')view.frame(data.ms);
      else if(data.type==='debug')log.setLevel(data.level);
      else if(data.type==='event')view.event(data.kind,data.identifier,data.x,data.y);
      else if(data.type==='resize'){view.resize(data.width,data.height);canvas.width=data.width;canvas.height=data.height;}
      else if(data.type==='test'){self.postMessage({id,result:smokeCheck(module,identity,hello)});return;}
      else if(data.type==='journal'){self.postMessage({id,result:view.journal()});return;}
      else if(data.type==='status'){self.postMessage({id,result:{...view.status(),image:{...identity,device:hello.device,firmware:hello.firmware}}});return;}
      else if(data.type==='dispose'){module.ccall('b8_wasm_dispose',null,[],[]);view=null;self.postMessage({id,result:{ok:true,closed:true}});return;}
      else throw new Error('Unknown scene request');
    }
    const {width,height,rgba}=view.pixels();
    // Only presentation: every RGBA value, label and shape was drawn by C++.
    ctx.putImageData(new ImageData(rgba,width,height),0,0);
    self.postMessage({id,result:{...view.status(),image:{...identity,device:hello.device,firmware:hello.firmware}}});
  }catch(error){failed=true;const record=log.emit('error','wasm.view_failure',{error});self.postMessage({id,fatal:true,error:`WASM_VIEW_FAILURE: ${error?.message??String(error)}`,details:record?.data});}
  finally{busy=false;}
});
